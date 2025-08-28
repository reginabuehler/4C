// This file is part of 4C multiphysics licensed under the
// GNU Lesser General Public License v3.0 or later.
//
// See the LICENSE.md file in the top-level for license information.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef FOUR_C_LINEAR_SOLVER_METHOD_DIRECT_HPP
#define FOUR_C_LINEAR_SOLVER_METHOD_DIRECT_HPP

#include "4C_config.hpp"

#include "4C_linalg_sparseoperator.hpp"
#include "4C_linear_solver_method.hpp"

#if FOUR_C_TRILINOS_INTERNAL_VERSION_GE(2025, 3)
#include <Amesos2.hpp>
#else
#include <Amesos_BaseSolver.h>
#include <EpetraExt_Reindex_LinearProblem2.h>
#endif

FOUR_C_NAMESPACE_OPEN

namespace Core::LinearSolver
{
  /// direct linear solver (using amesos)
  class DirectSolver : public SolverTypeBase
  {
   public:
    //! Constructor
    explicit DirectSolver(std::string solvertype);

    /*! \brief Setup the solver object
     *
     * @param A Matrix of the linear system
     * @param x Solution vector of the linear system
     * @param b Right-hand side vector of the linear system
     * @param refactor Boolean flag to enforce a refactorization of the matrix
     * @param reset Boolean flag to enforce a full reset of the solver object
     * @param projector Krylov projector
     */
    void setup(std::shared_ptr<Core::LinAlg::SparseOperator> matrix,
        std::shared_ptr<Core::LinAlg::MultiVector<double>> x,
        std::shared_ptr<Core::LinAlg::MultiVector<double>> b, const bool refactor, const bool reset,
        std::shared_ptr<Core::LinAlg::KrylovProjector> projector = nullptr) override;

    //! Actual call to the underlying amesos solver
    int solve() override;

    bool is_factored() { return factored_; }

   private:
    //! type/implementation of Amesos solver to be used
    const std::string solvertype_;

    //! flag indicating whether a valid factorization is stored
    bool factored_;

    //! initial guess and solution
    std::shared_ptr<Core::LinAlg::MultiVector<double>> x_;

    //! right hand side vector
    std::shared_ptr<Core::LinAlg::MultiVector<double>> b_;

    //! system of equations
    std::shared_ptr<Core::LinAlg::SparseMatrix> a_;

#if FOUR_C_TRILINOS_INTERNAL_VERSION_GE(2025, 3)
    //! an abstract Amesos2 solver that can be any of the concrete implementations
    Teuchos::RCP<Amesos2::Solver<Epetra_CrsMatrix, Epetra_MultiVector>> solver_;
#else
    //! a linear problem wrapper class used by Trilinos and for scaling of the system
    std::shared_ptr<Epetra_LinearProblem> linear_problem_;

    //! an abstract amesos solver that can be any of the amesos concrete implementations
    std::shared_ptr<Amesos_BaseSolver> solver_;

    //! reindex linear problem for amesos
    std::shared_ptr<EpetraExt::LinearProblem_Reindex2> reindexer_;
#endif

    /*! \brief Krylov projector for solving near singular linear systems
     *
     * Instead of solving Ax=b a projected system of the form P'APu=P'b is solved.
     * With P being the krylov projector.
     *
     * P. Bochev and R. B. Lehoucq: On the Finite Element Solution of the Pure Neumann Problem,
     * SIAM Review, 47(1):50-66, 2005, http://dx.doi.org/10.1137/S0036144503426074
     *
     */
    std::shared_ptr<Core::LinAlg::KrylovProjector> projector_;
  };
}  // namespace Core::LinearSolver

FOUR_C_NAMESPACE_CLOSE

#endif
