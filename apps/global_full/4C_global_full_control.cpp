// This file is part of 4C multiphysics licensed under the
// GNU Lesser General Public License v3.0 or later.
//
// See the LICENSE.md file in the top-level for license information.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "4C_comm_mpi_utils.hpp"
#include "4C_comm_utils.hpp"
#include "4C_global_data.hpp"
#include "4C_global_full_init_control.hpp"
#include "4C_global_full_inp_control.hpp"
#include "4C_io_pstream.hpp"

#include <chrono>
#include <thread>


namespace
{
  double walltime_in_seconds()
  {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::high_resolution_clock::now().time_since_epoch())
               .count() *
           1.0e-3;
  }

  void print_dat_deprecation_warning()
  {
    std::cout << "\n\n"
              << "-------------------------\n"
              << "-- DEPRECATION WARNING --\n"
              << "-------------------------\n"
              << "The use of the .dat file format is deprecated. Please use .yaml instead.\n"
              << "You can convert a .dat file to .yaml using 4C's --to-yaml option.\n"
              << std::endl;
  }
}  // namespace

void ntacal();

/*----------------------------------------------------------------------*
 | main routine                                           m.gee 8/00    |
 *----------------------------------------------------------------------*/
void ntam(int argc, char* argv[])
{
  using namespace FourC;

  MPI_Comm gcomm = Global::Problem::instance()->get_communicators()->global_comm();

  double t0, ti, tc;

  /// IO file names and kenners
  std::string inputfile_name;
  std::string outputfile_kenner;
  std::string restartfile_kenner;

  ntaini_ccadiscret(argc, argv, inputfile_name, outputfile_kenner, restartfile_kenner);

  /* input phase, input of all information */

  if (Core::Communication::my_mpi_rank(gcomm) == 0)
  {
    if (inputfile_name.ends_with(".dat"))
    {
      print_dat_deprecation_warning();
    }
  }

  t0 = walltime_in_seconds();

  ntainp_ccadiscret(inputfile_name, outputfile_kenner, restartfile_kenner);

  ti = walltime_in_seconds() - t0;
  Core::Communication::barrier(gcomm);
  if (Core::Communication::my_mpi_rank(gcomm) == 0)
  {
    Core::IO::cout << "\nTotal wall time for INPUT:       " << std::setw(10) << std::setprecision(3)
                   << std::scientific << ti << " sec \n\n";
  }
  Core::Communication::barrier(gcomm);

  /*--------------------------------------------------calculation phase */
  t0 = walltime_in_seconds();

  ntacal();

  tc = walltime_in_seconds() - t0;
  Core::Communication::barrier(gcomm);
  if (Core::Communication::my_mpi_rank(gcomm) == 0)
  {
    Core::IO::cout << "\nTotal wall time for CALCULATION: " << std::setw(10) << std::setprecision(3)
                   << std::scientific << tc << " sec \n\n";
    Core::IO::flush(Core::IO::cout);
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  Core::Communication::barrier(gcomm);

  // Let's print the deprecation warning again to increase the chance users will see it.
  if (Core::Communication::my_mpi_rank(gcomm) == 0)
  {
    if (inputfile_name.ends_with(".dat"))
    {
      print_dat_deprecation_warning();
    }
  }
}
