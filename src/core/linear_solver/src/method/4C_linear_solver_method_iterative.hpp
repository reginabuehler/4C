// This file is part of 4C multiphysics licensed under the
// GNU Lesser General Public License v3.0 or later.
//
// See the LICENSE.md file in the top-level for license information.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef FOUR_C_LINEAR_SOLVER_METHOD_ITERATIVE_HPP
#define FOUR_C_LINEAR_SOLVER_METHOD_ITERATIVE_HPP

#include "4C_config.hpp"

#include "4C_linalg_sparseoperator.hpp"
#include "4C_linear_solver_method.hpp"
#include "4C_linear_solver_preconditioner_type.hpp"
#include "4C_utils_exceptions.hpp"
#include "4C_utils_parameter_list.fwd.hpp"

#include <memory>

FOUR_C_NAMESPACE_OPEN

namespace Core::LinearSolver
{
  //! krylov subspace linear solvers with right-side preconditioning
  class IterativeSolver : public SolverTypeBase
  {
   public:
    //! Constructor
    IterativeSolver(MPI_Comm comm, Teuchos::ParameterList& params);

    /*! \brief Setup the solver object
     *
     * @param A Matrix of the linear system
     * @param x Solution vector of the linear system
     * @param b Right-hand side vector of the linear system
     * @param refactor Boolean flag to enforce a refactorization of the matrix
     * @param reset Boolean flag to enforce a full reset of the solver object
     * @param projector Krylov projector
     */
    void setup(std::shared_ptr<Core::LinAlg::SparseOperator> A,
        std::shared_ptr<Core::LinAlg::MultiVector<double>> x,
        std::shared_ptr<Core::LinAlg::MultiVector<double>> b, const bool refactor, const bool reset,
        std::shared_ptr<Core::LinAlg::KrylovProjector> projector) override;

    //! Actual call to the underlying Belos solver
    int solve() override;

    int ncall() { return ncall_; }

    //! return number of iterations
    int get_num_iters() const override { return numiters_; };

    Teuchos::ParameterList& params() const { return params_; }

   private:
    /*! \brief Check whether preconditioner will be reused
     *
     * The user can control reuse/recomputation of the preconditioner by setting appropriate input
     * arguments \c reuse and \c reset. In addition, contact mechanics problems perform some
     * additional checks since they require to rebuild the preconditioner when the active set has
     * changed.
     *
     * @param[in] reuse Parameter AZREUSE from parameter list
     * @param reset Force preconditioner to be rebuilt
     * @return Boolean flag to indicate whether preconditioner is reused (\c true) or has to be
     * re-computed (\c false)
     *
     * \sa check_reuse_status_of_active_set()
     */
    bool allow_reuse_preconditioner(const int reuse, const bool reset);

    /*! \brief Function for creating preconditioner object
     *
     * @param solverlist liner solver parameter list
     * @param projector Krylov projector
     */
    std::shared_ptr<Core::LinearSolver::PreconditionerTypeBase> create_preconditioner(
        Teuchos::ParameterList& solverlist,
        std::shared_ptr<Core::LinAlg::KrylovProjector> projector);

    //! a communicator
    MPI_Comm comm_;

    //! (internal) parameter lists
    Teuchos::ParameterList& params_;

    //! initial guess and solution
    std::shared_ptr<Core::LinAlg::MultiVector<double>> x_;

    //! right hand side vector
    std::shared_ptr<Core::LinAlg::MultiVector<double>> b_;

    //! system of equations
    std::shared_ptr<Core::LinAlg::SparseOperator> a_;

    //! counting how many times matrix was solved between resets
    int ncall_{0};

    //! number of iterations
    int numiters_{-1};

    //! preconditioner object
    std::shared_ptr<Core::LinearSolver::PreconditionerTypeBase> preconditioner_;

    /*! \brief Map of active DOFs in structural contact simulations.
     *
     * This is used to check whether preconditioner can be reused or not.
     * We cannot reuse the preconditioner if the active set has changed.
     *
     * \note Member variable only used for structural contact problems with the
     * "contact activeDofMap" parameter in the "Linear System properties"
     * parameter list set.
     *
     * \sa check_reuse_status_of_active_set()
     */
    std::shared_ptr<Core::LinAlg::Map> active_dof_map_;

    //!@}
  };
}  // namespace Core::LinearSolver

FOUR_C_NAMESPACE_CLOSE

#endif
