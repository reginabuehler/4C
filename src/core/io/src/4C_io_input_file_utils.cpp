// This file is part of 4C multiphysics licensed under the
// GNU Lesser General Public License v3.0 or later.
//
// See the LICENSE.md file in the top-level for license information.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "4C_config_revision.hpp"

#include "4C_io_input_file_utils.hpp"

#include "4C_fem_discretization.hpp"
#include "4C_fem_general_node.hpp"
#include "4C_fem_nurbs_discretization_knotvector.hpp"
#include "4C_io_input_file.hpp"
#include "4C_io_input_parameter_container.hpp"
#include "4C_io_input_spec.hpp"
#include "4C_io_pstream.hpp"
#include "4C_io_value_parser.hpp"
#include "4C_io_yaml.hpp"
#include "4C_linalg_utils_densematrix_communication.hpp"
#include "4C_utils_exceptions.hpp"
#include "4C_utils_string.hpp"

#include <Teuchos_ParameterList.hpp>
#include <Teuchos_StrUtils.hpp>
#include <Teuchos_Time.hpp>

#include <cfenv>

FOUR_C_NAMESPACE_OPEN

void Core::IO::print_section_header(std::ostream& out, const std::string& header)
{
  constexpr std::size_t max_padding = 65ul;
  const std::size_t padding = (header.length() < max_padding) ? (max_padding - header.length()) : 0;

  out << "--" << std::string(padding, '-') << header << '\n';
}



void Core::IO::print_section(std::ostream& out, const std::string& header, const InputSpec& spec)
{
  print_section_header(out, header);
  spec.print_as_dat(out);
}


std::pair<std::string, std::string> Core::IO::read_key_value(const std::string& line)
{
  std::string::size_type separator_index = line.find('=');
  // The equals sign is only treated as a separator when surrounded by whitespace.
  if (separator_index != std::string::npos &&
      !(std::isspace(line[separator_index - 1]) && std::isspace(line[separator_index + 1])))
    separator_index = std::string::npos;

  // In case we didn't find an "=" separator, look for a space instead
  if (separator_index == std::string::npos)
  {
    separator_index = line.find(' ');

    if (separator_index == std::string::npos)
      FOUR_C_THROW("Line '{}' with just one word in parameter section", line);
  }

  std::string key = Core::Utils::trim(line.substr(0, separator_index));
  std::string value = Core::Utils::trim(line.substr(separator_index + 1));

  if (key.empty()) FOUR_C_THROW("Cannot get key from line '{}'", line);
  if (value.empty()) FOUR_C_THROW("Cannot get value from line '{}'", line);

  return {std::move(key), std::move(value)};
}


namespace
{
  /*----------------------------------------------------------------------*/
  /*----------------------------------------------------------------------*/
  Teuchos::ParameterList& find_sublist(std::string name, Teuchos::ParameterList& list)
  {
    Teuchos::ParameterList* sublist = &list;

    for (std::string::size_type pos = name.find('/'); pos != std::string::npos;
        pos = name.find('/'))
    {
      sublist = &sublist->sublist(name.substr(0, pos));
      name = name.substr(pos + 1);
    }

    return sublist->sublist(name);
  }

}  // namespace



/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void Core::IO::read_parameters_in_section(
    InputFile& input, const std::string& section_name, Teuchos::ParameterList& list)
{
  if (section_name.empty()) FOUR_C_THROW("Empty section name given.");

  InputParameterContainer container;
  input.match_section(section_name, container);

  if (container.has_group(section_name))
  {
    // This special case should go away when sections with "/" are no longer present
    container.group(section_name).to_teuchos_parameter_list(find_sublist(section_name, list));
  }
  else
  {
    container.to_teuchos_parameter_list(list);
  }
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void Core::IO::read_design(InputFile& input, const std::string& name,
    std::vector<std::vector<int>>& dobj_fenode,
    const std::function<const Core::FE::Discretization&(const std::string& name)>&
        get_discretization)
{
  std::map<int, std::set<int>> topology;

  std::string sectionname = name + "-NODE TOPOLOGY";
  std::string marker = sectionname;

  // Store lines that need special treatment
  std::vector<std::string> box_face_conditions;

  const auto error_scope = "While reading design nodes in section '" + marker + "': ";
  for (const auto& l : input.in_section_rank_0_only(marker))
  {
    ValueParser parser{l.get_as_dat_style_string(), {.user_scope_message = error_scope}};
    const std::string parsed_name = parser.read<std::string>();

    if (parsed_name == "NODE")  // plain old reading of the design nodes from the input file
    {
      const int nodeid = parser.read<int>();
      const std::string dname = parser.read<std::string>();
      const int dobj = parser.read<int>();

      FOUR_C_ASSERT_ALWAYS(name == dname || (name == "DSURF" && dname == "DSURFACE") ||
                               (name == "DVOL" && dname == "DVOLUME"),
          "Wrong design node name: {}. Expected {}.", dname, name);

      topology[dobj - 1].insert(nodeid - 1);

      FOUR_C_ASSERT_ALWAYS(parser.at_end(),
          "Illegal line in section '{}': '{}' has unparsed remainder '{}'", marker,
          l.get_as_dat_style_string(), parser.get_unparsed_remainder());
    }
    else  // fancy specification of the design nodes by specifying min or max of the domain
    {     // works best on rectangular domains ;)
      // Store the specification and broadcast it to all processors
      box_face_conditions.emplace_back(l.get_as_dat_style_string());
    }
  }

  // Broadcast what we have read on rank 0 to all other ranks
  Core::Communication::broadcast(topology, 0, input.get_comm());
  Core::Communication::broadcast(box_face_conditions, 0, input.get_comm());

  // Special treatment if we have any box face conditions
  {
    for (const auto& l : box_face_conditions)
    {
      int dobj;
      std::string nname;
      std::string dname;
      std::string disname;
      std::array<int, 3> dir = {0, 0, 0};

      std::istringstream stream{l};
      stream >> nname;
      if (not stream) FOUR_C_THROW("Illegal line in section '{}': '{}'", marker, l.data());

      if (nname == "CORNER" && name == "DNODE")
      {
        std::string tmp;
        stream >> disname;
        for (int i = 0; i < 3; ++i)
        {
          stream >> tmp;
          if (tmp.size() != 2 || tmp[0] < 'x' || tmp[0] > 'z' || (tmp[1] != '+' && tmp[1] != '-'))
            FOUR_C_THROW("Illegal design node definition.");
          dir[tmp[0] - 'x'] = (tmp[1] == '+') ? 1 : -1;
        }
        stream >> dname >> dobj;
      }
      else if (nname == "EDGE" && name == "DLINE")
      {
        std::string tmp;
        stream >> disname;
        for (int i = 0; i < 2; ++i)
        {
          stream >> tmp;
          if (tmp.size() != 2 || tmp[0] < 'x' || tmp[0] > 'z' || (tmp[1] != '+' && tmp[1] != '-'))
            FOUR_C_THROW("Illegal design node definition.");
          dir[tmp[0] - 'x'] = (tmp[1] == '+') ? 1 : -1;
        }
        stream >> dname >> dobj;
      }
      else if (nname == "SIDE" && name == "DSURF")
      {
        std::string tmp;
        stream >> disname;
        stream >> tmp;
        if (tmp.size() != 2 || tmp[0] < 'x' || tmp[0] > 'z' || (tmp[1] != '+' && tmp[1] != '-'))
          FOUR_C_THROW("Illegal design node definition.");
        dir[tmp[0] - 'x'] = (tmp[1] == '+') ? 1 : -1;
        stream >> dname >> dobj;
      }
      else if (nname == "VOLUME" && name == "DVOL")
      {
        stream >> disname;
        stream >> dname >> dobj;
      }
      else
      {
        FOUR_C_THROW("Illegal line in section '{}': '{}'", marker, l.data());
      }

      const Core::FE::Discretization& actdis = get_discretization(disname);

      std::vector<double> box_specifications;
      box_specifications.reserve(9);  // 3 coords, 3 coords, 3 rotations
      {
        if (Core::Communication::my_mpi_rank(input.get_comm()) == 0)  // Reading is done by proc 0
        {
          // get original domain section from the input file
          std::string domain_section_name = disname + " DOMAIN";
          std::ranges::transform(domain_section_name, domain_section_name.begin(), ::toupper);

          InputParameterContainer container;
          input.match_section(domain_section_name, container);
          const auto& domain_data = container.group(domain_section_name);

          const auto append_to_box_specifications = [&box_specifications](
                                                        const std::vector<double>& values)
          {
            FOUR_C_ASSERT_ALWAYS(
                values.size() == 3, "Internal error: exactly three values expected.");
            box_specifications.insert(box_specifications.end(), values.begin(), values.end());
          };

          append_to_box_specifications(domain_data.get<std::vector<double>>("bottom_corner_point"));
          append_to_box_specifications(domain_data.get<std::vector<double>>("top_corner_point"));
          append_to_box_specifications(domain_data.get<std::vector<double>>("rotation_angle"));
        }
        else  // All other processors get an empty vector
        {
          box_specifications.resize(9, 0.0);
        }

        // All other processors get this info broadcasted
        Core::Communication::broadcast(box_specifications.data(),
            static_cast<int>(box_specifications.size()), 0, input.get_comm());
      }

      // determine the active discretizations bounding box
      std::array<double, 6> bbox;
      for (size_t i = 0; i < sizeof(bbox) / sizeof(bbox[0]); ++i) bbox[i] = box_specifications[i];

      constexpr double tolerance_n = 1.0e-14;
      // manipulate the bounding box according to the specified condition
      for (size_t i = 0; i < 3; ++i)
      {
        switch (dir[i])
        {
          case 0:
            bbox[i + 0] = std::numeric_limits<double>::max();
            bbox[i + 3] = -std::numeric_limits<double>::max();
            break;
          case -1:
            bbox[i] += tolerance_n;
            bbox[i + 3] = std::numeric_limits<double>::max();
            break;
          case 1:
            bbox[i] = -std::numeric_limits<double>::max();
            bbox[i + 3] -= tolerance_n;
            break;
          default:
            FOUR_C_THROW("Invalid BC specification");
        }
      }

      // collect all nodes which are outside the adapted bounding box
      std::set<int> dnodes;
      for (const auto* node : actdis.my_row_node_range())
      {
        const auto& coord = node->x();
        std::array<double, 3> coords;
        coords[0] = coord[0];
        coords[1] = coord[1];
        coords[2] = coord[2];
        // rotate back to identify condition, if a rotation is defined
        static const int rotoffset = 6;
        for (int rotaxis = 2; rotaxis > -1; --rotaxis)
        {
          if (box_specifications[rotaxis + rotoffset] != 0.0)
          {
            std::array<double, 3> coordm;
            coordm[0] = (box_specifications[0] + box_specifications[3]) / 2.;
            coordm[1] = (box_specifications[1] + box_specifications[4]) / 2.;
            coordm[2] = (box_specifications[2] + box_specifications[5]) / 2.;
            // add rotation around mitpoint here.
            std::array<double, 3> dx;
            dx[0] = coords[0] - coordm[0];
            dx[1] = coords[1] - coordm[1];
            dx[2] = coords[2] - coordm[2];

            double calpha = cos(-box_specifications[rotaxis + rotoffset] * std::numbers::pi / 180);
            double salpha = sin(-box_specifications[rotaxis + rotoffset] * std::numbers::pi / 180);

            coords[0] = coordm[0];  //+ calpha*dx[0] + salpha*dx[1];
            coords[1] = coordm[1];  //+ -salpha*dx[0] + calpha*dx[1];
            coords[2] = coordm[2];

            coords[(rotaxis + 1) % 3] +=
                calpha * dx[(rotaxis + 1) % 3] + salpha * dx[(rotaxis + 2) % 3];
            coords[(rotaxis + 2) % 3] +=
                calpha * dx[(rotaxis + 2) % 3] - salpha * dx[(rotaxis + 1) % 3];
            coords[rotaxis] += dx[rotaxis];
          }
        }

        if ((coords[0] <= bbox[0] || coords[0] >= bbox[3]) &&
            (coords[1] <= bbox[1] || coords[1] >= bbox[4]) &&
            (coords[2] <= bbox[2] || coords[2] >= bbox[5]))
          dnodes.insert(node->id());
      }
      Core::LinAlg::gather_all(dnodes, input.get_comm());
      topology[dobj - 1].insert(dnodes.begin(), dnodes.end());


      if (dname.substr(0, name.length()) != name)
        FOUR_C_THROW("Illegal line in section '{}': '{}'\n{} found, where {} was expected",
            marker.c_str(), l.data(), dname.substr(0, name.length()), name);
    }
  }

  if (topology.size() > 0)
  {
    int max_num_dobj = topology.rbegin()->first;
    if (max_num_dobj >= static_cast<int>(dobj_fenode.size())) dobj_fenode.resize(max_num_dobj + 1);
    // copy all design object entries
    for (auto& topo : topology)
    {
      // we copy from a std::set, thus the gids are sorted
      dobj_fenode[topo.first].reserve(topo.second.size());
      dobj_fenode[topo.first].assign(topo.second.begin(), topo.second.end());
    }
  }
}


std::unique_ptr<Core::FE::Nurbs::Knotvector> Core::IO::read_knots(
    InputFile& input, const std::string& name)
{
  std::string field;
  if (name == "fluid" || name == "xfluid" || name == "porofluid")
    field = "FLUID";
  else if (name == "structure")
    field = "STRUCTURE";
  else if (name == "ale")
    field = "ALE";
  else if (name == "scatra")
    field = "TRANSPORT";
  else if (name == "thermo")
    field = "THERMO";
  else if (name == "scatra_micro")
    field = "TRANSPORT2";
  else
    FOUR_C_THROW("Unknown discretization name for knotvector input.");

  const std::string sectionname = field + " KNOTVECTORS";

  InputParameterContainer data;
  input.match_section(sectionname, data);
  return std::make_unique<Core::FE::Nurbs::Knotvector>(
      FE::Nurbs::Knotvector::from_input(data.group(sectionname)));
}

FOUR_C_NAMESPACE_CLOSE
