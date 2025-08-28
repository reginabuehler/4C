// This file is part of 4C multiphysics licensed under the
// GNU Lesser General Public License v3.0 or later.
//
// See the LICENSE.md file in the top-level for license information.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef SOLVER_NONLIN_NOX_SOLVER_NONLIN_NOX_PROBLEM_CPP
#define SOLVER_NONLIN_NOX_SOLVER_NONLIN_NOX_PROBLEM_CPP

#include "4C_solver_nonlin_nox_problem.hpp"  // class definition

#include "4C_linear_solver_method_linalg.hpp"
#include "4C_solver_nonlin_nox_aux.hpp"
#include "4C_solver_nonlin_nox_constraint_group.hpp"
#include "4C_solver_nonlin_nox_constraint_interface_required.hpp"
#include "4C_solver_nonlin_nox_globaldata.hpp"
#include "4C_solver_nonlin_nox_inner_statustest_factory.hpp"
#include "4C_solver_nonlin_nox_interface_jacobian.hpp"
#include "4C_solver_nonlin_nox_linearsystem.hpp"
#include "4C_solver_nonlin_nox_linearsystem_factory.hpp"
#include "4C_solver_nonlin_nox_scaling.hpp"
#include "4C_solver_nonlin_nox_singlestep_group.hpp"

#include <NOX_Epetra_Interface_Required.H>
#include <NOX_Epetra_Vector.H>
#include <NOX_StatusTest_Generic.H>
#include <Teuchos_ParameterList.hpp>

FOUR_C_NAMESPACE_OPEN


/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
NOX::Nln::Problem::Problem(const Teuchos::RCP<NOX::Nln::GlobalData>& noxNlnGlobalData)
    : isinit_(false),
      isjac_(false),
      noxNlnGlobalData_(noxNlnGlobalData),
      xVector_(nullptr),
      jac_(nullptr),
      precMat_(Teuchos::null)
{ /* intentionally left blank */
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
NOX::Nln::Problem::Problem(const Teuchos::RCP<NOX::Nln::GlobalData>& noxNlnGlobalData,
    const Teuchos::RCP<::NOX::Epetra::Vector>& x,
    const Teuchos::RCP<Core::LinAlg::SparseOperator>& A)
    : isinit_(false),
      noxNlnGlobalData_(noxNlnGlobalData),
      xVector_(nullptr),
      jac_(nullptr),
      precMat_(Teuchos::null)
{
  initialize(x, A);
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
void NOX::Nln::Problem::initialize(const Teuchos::RCP<::NOX::Epetra::Vector>& x,
    const Teuchos::RCP<Core::LinAlg::SparseOperator>& A)
{
  // in the standard case, we use the input rhs and matrix
  // ToDo Check if CreateView is sufficient
  if (x.is_null())
    FOUR_C_THROW("You have to provide a state vector pointer unequal Teuchos::null!");

  xVector_ = &x;
  isjac_ = (not A.is_null());
  jac_ = &A;

  isinit_ = true;
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
Teuchos::RCP<::NOX::Epetra::LinearSystem> NOX::Nln::Problem::create_linear_system() const
{
  check_init();
  if (not is_jacobian())
    FOUR_C_THROW(
        "You have to set a jacobian first, before you can create a "
        "linear system!");

  const NOX::Nln::LinSystem::LinearSystemType linsystype =
      NOX::Nln::Aux::get_linear_system_type(noxNlnGlobalData_->get_linear_solvers());
  std::shared_ptr<NOX::Nln::Scaling> scalingObject = noxNlnGlobalData_->get_scaling_object();

  // build the linear system --> factory call
  return NOX::Nln::LinSystem::build_linear_system(
      linsystype, *noxNlnGlobalData_, *jac_, **xVector_, precMat_, scalingObject);
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
Teuchos::RCP<::NOX::Abstract::Group> NOX::Nln::Problem::create_group(
    const Teuchos::RCP<::NOX::Epetra::LinearSystem>& linSys) const
{
  check_init();
  Teuchos::RCP<::NOX::Abstract::Group> noxgrp = Teuchos::null;

  Teuchos::ParameterList& params = noxNlnGlobalData_->get_nln_parameter_list();
  const Teuchos::RCP<::NOX::Epetra::Interface::Required>& iReq =
      noxNlnGlobalData_->get_required_interface();
  const std::string nlnSolver = params.get<std::string>("Nonlinear Solver", "");
  if (noxNlnGlobalData_->is_constrained())
  {
    const NOX::Nln::CONSTRAINT::ReqInterfaceMap& iconstr =
        noxNlnGlobalData_->get_constraint_interfaces();
    noxgrp = Teuchos::make_rcp<NOX::Nln::CONSTRAINT::Group>(params.sublist("Printing"),
        params.sublist("Group Options"), iReq, **xVector_, linSys, iconstr);
  }
  else if (nlnSolver.compare("Single Step") == 0)
  {
    std::cout << "Single Step Group is selected" << std::endl;
    noxgrp = Teuchos::make_rcp<NOX::Nln::SINGLESTEP::Group>(
        params.sublist("Printing"), params.sublist("Group Options"), iReq, **xVector_, linSys);
  }
  else
  {
    noxgrp = Teuchos::make_rcp<NOX::Nln::Group>(
        params.sublist("Printing"), params.sublist("Group Options"), iReq, **xVector_, linSys);
  }

  return noxgrp;
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
void NOX::Nln::Problem::create_outer_status_test(
    Teuchos::RCP<::NOX::StatusTest::Generic>& outerTests) const
{
  Teuchos::ParameterList& p = noxNlnGlobalData_->get_nln_parameter_list();

  // A "Outer Status Test" has to be supplied by the user
  Teuchos::ParameterList& oParams =
      p.sublist("Status Test", true).sublist("Outer Status Test", true);
  outerTests =
      NOX::Nln::StatusTest::build_outer_status_tests(oParams, noxNlnGlobalData_->get_nox_utils());
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
void NOX::Nln::Problem::create_status_tests(Teuchos::RCP<::NOX::StatusTest::Generic>& outerTest,
    Teuchos::RCP<NOX::Nln::Inner::StatusTest::Generic>& innerTest) const
{
  create_outer_status_test(outerTest);

  // A "Inner Status Test" is optional in some cases.
  // Check if there is a "Inner Status Test" sublist and if it is filled.
  Teuchos::ParameterList& p = noxNlnGlobalData_->get_nln_parameter_list();
  if (p.sublist("Status Test", true).isSublist("Inner Status Test") and
      p.sublist("Status Test").sublist("Inner Status Test").numParams() != 0)
  {
    Teuchos::ParameterList& iParams = p.sublist("Status Test", true).sublist("Inner Status Test");
    innerTest = NOX::Nln::Inner::StatusTest::build_inner_status_tests(
        iParams, noxNlnGlobalData_->get_nox_utils());
  }

  return;
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
void NOX::Nln::Problem::check_final_status(const ::NOX::StatusTest::StatusType& finalStatus) const
{
  if (finalStatus != ::NOX::StatusTest::Converged)
  {
    FOUR_C_THROW("The nonlinear solver did not converge!");
  }

  return;
}

#endif

FOUR_C_NAMESPACE_CLOSE
