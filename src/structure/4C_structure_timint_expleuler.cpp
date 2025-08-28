// This file is part of 4C multiphysics licensed under the
// GNU Lesser General Public License v3.0 or later.
//
// See the LICENSE.md file in the top-level for license information.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "4C_structure_timint_expleuler.hpp"

#include "4C_contact_meshtying_contact_bridge.hpp"
#include "4C_fem_discretization.hpp"
#include "4C_io.hpp"
#include "4C_linalg_utils_sparse_algebra_create.hpp"
#include "4C_linear_solver_method_linalg.hpp"
#include "4C_mortar_manager_base.hpp"
#include "4C_mortar_strategy_base.hpp"

FOUR_C_NAMESPACE_OPEN

/*----------------------------------------------------------------------*/
/* Constructor */
Solid::TimIntExplEuler::TimIntExplEuler(const Teuchos::ParameterList& timeparams,
    const Teuchos::ParameterList& ioparams, const Teuchos::ParameterList& sdynparams,
    const Teuchos::ParameterList& xparams, std::shared_ptr<Core::FE::Discretization> actdis,
    std::shared_ptr<Core::LinAlg::Solver> solver,
    std::shared_ptr<Core::LinAlg::Solver> contactsolver,
    std::shared_ptr<Core::IO::DiscretizationWriter> output)
    : TimIntExpl(timeparams, ioparams, sdynparams, xparams, actdis, solver, contactsolver, output),
      modexpleuler_(sdynparams.get<bool>("MODIFIEDEXPLEULER")),
      fextn_(nullptr),
      fintn_(nullptr),
      fviscn_(nullptr),
      fcmtn_(nullptr),
      frimpn_(nullptr)
{
  // Keep this constructor empty!
  // First do everything on the more basic objects like the discretizations, like e.g.
  // redistribution of elements. Only then call the setup to this class. This will call the setup to
  // all classes in the inheritance hierarchy. This way, this class may also override a method that
  // is called during setup() in a base class.
  return;
}

/*----------------------------------------------------------------------------------------------*
 * Initialize this class                                                            rauch 09/16 |
 *----------------------------------------------------------------------------------------------*/
void Solid::TimIntExplEuler::init(const Teuchos::ParameterList& timeparams,
    const Teuchos::ParameterList& sdynparams, const Teuchos::ParameterList& xparams,
    std::shared_ptr<Core::FE::Discretization> actdis, std::shared_ptr<Core::LinAlg::Solver> solver)
{
  // call init() in base class
  Solid::TimIntExpl::init(timeparams, sdynparams, xparams, actdis, solver);


  // info to user
  if (myrank_ == 0)
  {
    std::cout << "with " << (modexpleuler_ ? "modified" : "standard") << " forward Euler"
              << std::endl
              << "lumping activated: " << (lumpmass_ ? "true" : "false") << std::endl
              << std::endl;
  }

  // let it rain
  return;
}

/*----------------------------------------------------------------------------------------------*
 * Setup this class                                                                 rauch 09/16 |
 *----------------------------------------------------------------------------------------------*/
void Solid::TimIntExplEuler::setup()
{
  // call setup() in base class
  Solid::TimIntExpl::setup();

  // determine mass, damping and initial accelerations
  determine_mass_damp_consist_accel();

  // resize of multi-step quantities
  resize_m_step();

  // allocate force vectors
  fextn_ = Core::LinAlg::create_vector(*dof_row_map_view(), true);
  fintn_ = Core::LinAlg::create_vector(*dof_row_map_view(), true);
  fviscn_ = Core::LinAlg::create_vector(*dof_row_map_view(), true);
  fcmtn_ = Core::LinAlg::create_vector(*dof_row_map_view(), true);
  frimpn_ = Core::LinAlg::create_vector(*dof_row_map_view(), true);

  return;
}

/*----------------------------------------------------------------------*/
/* Resizing of multi-step quantities */
void Solid::TimIntExplEuler::resize_m_step()
{
  // nothing to do, because ExplEuler is a 1-step method
  return;
}

/*----------------------------------------------------------------------*/
/* Integrate step */
int Solid::TimIntExplEuler::integrate_step()
{
  // things to be done before integrating
  pre_solve();

  // time this step
  timer_->reset();

  const double dt = (*dt_)[0];  // \f$\Delta t_{n}\f$

  // new velocities \f$V_{n+1}\f$
  veln_->update(1.0, *(*vel_)(0), 0.0);
  veln_->update(dt, *(*acc_)(0), 1.0);

  // new displacements \f$D_{n+1}\f$, modified expl Euler uses veln_ for
  // updating disn_
  disn_->update(1.0, *(*dis_)(0), 0.0);
  if (modexpleuler_ == true)
    disn_->update(dt, *veln_, 1.0);
  else
    disn_->update(dt, *(*vel_)(0), 1.0);

  // *********** time measurement ***********
  double dtcpu = timer_->wallTime();
  // *********** time measurement ***********

  // apply Dirichlet BCs
  apply_dirichlet_bc(timen_, disn_, veln_, nullptr, false);

  // initialise stiffness matrix to zero
  stiff_->zero();

  // build new external forces
  fextn_->put_scalar(0.0);
  apply_force_external(timen_, disn_, veln_, *fextn_);

  // additional external forces are added (e.g. interface forces)
  fextn_->update(1.0, *fifc_, 1.0);

  // TIMING
  // double dtcpu = timer_->wallTime();

  // initialise internal forces
  fintn_->put_scalar(0.0);

  // ordinary internal force and stiffness
  {
    // displacement increment in step
    Core::LinAlg::Vector<double> disinc = Core::LinAlg::Vector<double>(*disn_);
    disinc.update(-1.0, *(*dis_)(0), 1.0);
    // internal force
    apply_force_internal(
        timen_, dt, disn_, Core::Utils::shared_ptr_from_ref(disinc), veln_, fintn_);
  }

  // *********** time measurement ***********
  dtele_ = timer_->wallTime() - dtcpu;
  // *********** time measurement ***********

  // viscous forces due Rayleigh damping
  if (damping_ == Inpar::Solid::damp_rayleigh)
  {
    damp_->multiply(false, *veln_, *fviscn_);
  }

  // *********** time measurement ***********
  dtcpu = timer_->wallTime();
  // *********** time measurement ***********

  // contact or meshtying forces
  if (have_contact_meshtying())
  {
    fcmtn_->put_scalar(0.0);

    if (cmtbridge_->have_meshtying())
      cmtbridge_->mt_manager()->get_strategy().apply_force_stiff_cmt(
          disn_, stiff_, fcmtn_, stepn_, 0, false);
    if (cmtbridge_->have_contact())
      cmtbridge_->contact_manager()->get_strategy().apply_force_stiff_cmt(
          disn_, stiff_, fcmtn_, stepn_, 0, false);
  }

  // *********** time measurement ***********
  dtcmt_ = timer_->wallTime() - dtcpu;
  // *********** time measurement ***********

  // determine time derivative of linear momentum vector,
  // ie \f$\dot{P} = M \dot{V}_{n=1}\f$
  frimpn_->update(1.0, *fextn_, -1.0, *fintn_, 0.0);

  if (damping_ == Inpar::Solid::damp_rayleigh)
  {
    frimpn_->update(-1.0, *fviscn_, 1.0);
  }

  if (have_contact_meshtying())
  {
    frimpn_->update(1.0, *fcmtn_, 1.0);
  }

  // *********** time measurement ***********
  dtcpu = timer_->wallTime();
  // *********** time measurement ***********

  // obtain new accelerations \f$A_{n+1}\f$
  {
    FOUR_C_ASSERT(mass_->filled(), "Mass matrix has to be completed");
    // blank linear momentum zero on DOFs subjected to DBCs
    dbcmaps_->insert_cond_vector(*dbcmaps_->extract_cond_vector(*zeros_), *frimpn_);
    // get accelerations
    accn_->put_scalar(0.0);

    // in case of no lumping or if mass matrix is a BlockSparseMatrix, use solver
    if (lumpmass_ == false ||
        std::dynamic_pointer_cast<Core::LinAlg::SparseMatrix>(mass_) == nullptr)
    {
      // refactor==false: This is not necessary, because we always
      // use the same constant mass matrix, which was firstly factorised
      // in TimInt::determine_mass_damp_consist_accel
      Core::LinAlg::SolverParams solver_params;
      solver_params.reset = true;
      solver_->solve(mass_, accn_, frimpn_, solver_params);
    }
    // direct inversion based on lumped mass matrix
    else
    {
      // extract the diagonal values of the mass matrix
      std::shared_ptr<Core::LinAlg::Vector<double>> diag = Core::LinAlg::create_vector(
          (std::dynamic_pointer_cast<Core::LinAlg::SparseMatrix>(mass_))->row_map());
      (std::dynamic_pointer_cast<Core::LinAlg::SparseMatrix>(mass_))->extract_diagonal_copy(*diag);
      // A_{n+1} = M^{-1} . ( -fint + fext )
      accn_->reciprocal_multiply(1.0, *diag, *frimpn_, 0.0);
    }
  }

  // apply Dirichlet BCs on accelerations
  apply_dirichlet_bc(timen_, nullptr, nullptr, accn_, false);

  // *********** time measurement ***********
  dtsolve_ = timer_->wallTime() - dtcpu;
  // *********** time measurement ***********

  // wassup?
  return 0;
}

/*----------------------------------------------------------------------*/
/* Update step */
void Solid::TimIntExplEuler::update_step_state()
{
  // new displacements at t_{n+1} -> t_n
  //    D_{n} := D_{n+1}
  dis_->update_steps(*disn_);
  // new velocities at t_{n+1} -> t_n
  //    V_{n} := V_{n+1}
  vel_->update_steps(*veln_);
  // new accelerations at t_{n+1} -> t_n
  //    A_{n} := A_{n+1}
  acc_->update_steps(*accn_);

  // update contact and meshtying
  update_step_contact_meshtying();

  // bye
  return;
}

/*----------------------------------------------------------------------*/
/* update after time step after output on element level*/
// update anything that needs to be updated at the element level
void Solid::TimIntExplEuler::update_step_element()
{
  // create the parameters for the discretization
  Teuchos::ParameterList p;
  // other parameters that might be needed by the elements
  p.set("total time", timen_);
  p.set("delta time", (*dt_)[0]);
  // action for elements
  p.set("action", "calc_struct_update_istep");
  // go to elements
  discret_->evaluate(p, nullptr, nullptr, nullptr, nullptr, nullptr);
}

/*----------------------------------------------------------------------*/
/* read restart forces */
void Solid::TimIntExplEuler::read_restart_force() { return; }

/*----------------------------------------------------------------------*/
/* write internal and external forces for restart */
void Solid::TimIntExplEuler::write_restart_force(
    std::shared_ptr<Core::IO::DiscretizationWriter> output)
{
  return;
}

FOUR_C_NAMESPACE_CLOSE
