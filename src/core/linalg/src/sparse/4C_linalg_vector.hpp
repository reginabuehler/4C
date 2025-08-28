// This file is part of 4C multiphysics licensed under the
// GNU Lesser General Public License v3.0 or later.
//
// See the LICENSE.md file in the top-level for license information.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef FOUR_C_LINALG_VECTOR_HPP
#define FOUR_C_LINALG_VECTOR_HPP

#include "4C_config.hpp"

#include "4C_linalg_map.hpp"
#include "4C_linalg_multi_vector.hpp"
#include "4C_linalg_transfer.hpp"
#include "4C_linalg_view.hpp"

#include <Epetra_IntVector.h>
#include <Epetra_Vector.h>

#include <memory>

FOUR_C_NAMESPACE_OPEN

namespace Core::LinAlg
{

  // Sparse Vector which will replace the Epetra_Vector
  template <typename T>
  class Vector
  {
    static_assert(std::is_same_v<T, double>, "Only double is supported for now");

   public:
    /// Basic vector constructor to create vector based on a map and initialize memory with zeros
    explicit Vector(const Epetra_BlockMap& Map, bool zeroOut = true);

    explicit Vector(const Map& Map, bool zeroOut = true);

    /// Copy constructor from epetra to vector
    explicit Vector(const Epetra_Vector& Source);

    explicit Vector(const Epetra_FEVector& Source);

    // Rule of five: We currently need to take care to make a deep copy of the Epetra_Vector.
    Vector(const Vector& other);

    Vector& operator=(const Vector& other);

    ~Vector() = default;

    // Implicit conversion to MultiVector: the MultiVector will view the same content and only have
    // a single column.
    operator const MultiVector<T>&() const;
    operator MultiVector<T>&();

    // Explicit conversion to MultiVector: the MultiVector will view the same content and only have
    // a single column.
    const MultiVector<T>& as_multi_vector() const;
    MultiVector<T>& as_multi_vector();

    // (Implicit) conversions: they all return references or RCPs, never copies
    const Epetra_Vector& get_ref_of_epetra_vector() const { return *vector_; }

    Epetra_Vector& get_ref_of_epetra_vector() { return *vector_; }

    operator Epetra_MultiVector&() { return *vector_; }

    operator const Epetra_MultiVector&() const { return *vector_; }

    operator Epetra_Vector&() { return *vector_; }

    operator const Epetra_Vector&() const { return *vector_; }

    //! Computes dot product of each corresponding pair of vectors.
    int dot(const Epetra_MultiVector& A, double* Result) const;

    //! Puts element-wise absolute values of input Multi-vector in target.
    int abs(const Epetra_MultiVector& A);

    //! Replace multi-vector values with scaled values of A, \e this = ScalarA*A.
    int scale(double ScalarA, const Epetra_MultiVector& A);

    //! Update multi-vector values with scaled values of A, \e this = ScalarThis*\e this +
    //! ScalarA*A.
    int update(double ScalarA, const Epetra_MultiVector& A, double ScalarThis);

    //! Update multi-vector with scaled values of A and B, \e this = ScalarThis*\e this + ScalarA*A
    //! + ScalarB*B.
    int update(double ScalarA, const Epetra_MultiVector& A, double ScalarB,
        const Epetra_MultiVector& B, double ScalarThis);

    //! Compute 1-norm of each vector
    int norm_1(double* Result) const;

    //! Compute 2-norm of each vector
    int norm_2(double* Result) const;

    //! Compute Inf-norm of each vector
    int norm_inf(double* Result) const;

    //! Compute minimum value of each vector
    int min_value(double* Result) const;

    //! Compute maximum value of each vector
    int max_value(double* Result) const;

    //! Compute mean (average) value of each vector
    int mean_value(double* Result) const;

    //! Scale the current values of a multi-vector, \e this = ScalarValue*\e this.
    int scale(double ScalarValue) { return vector_->Scale(ScalarValue); }

    //! Computes dot product of each corresponding pair of vectors.
    int dot(const Vector& A, double* Result) const;

    //! Puts element-wise absolute values of input Multi-vector in target.
    int abs(const Vector& A);

    //! Replace multi-vector values with scaled values of A, \e this = ScalarA*A.
    int scale(double ScalarA, const Vector& A);

    //! Update multi-vector values with scaled values of A, \e this = ScalarThis*\e this +
    //! ScalarA*A.
    int update(double ScalarA, const Vector& A, double ScalarThis);

    //! Update multi-vector with scaled values of A and B, \e this = ScalarThis*\e this + ScalarA*A
    //! + ScalarB*B.
    int update(double ScalarA, const Vector& A, double ScalarB, const Vector& B, double ScalarThis);

    //! Initialize all values in a multi-vector with const value.
    int put_scalar(double ScalarConstant);

    //! Element access function
    double operator[](int const index) const { return (*vector_)[index]; }

    //! Returns the address of the Core::LinAlg::Map for this multi-vector.
    const Map& get_map() const;

    //! Returns the MPI_Comm for this multi-vector.
    MPI_Comm get_comm() const;

    //! Returns true if this multi-vector is distributed global, i.e., not local replicated.
    bool distributed_global() const { return (vector_->Map().DistributedGlobal()); };

    //! Print method
    void print(std::ostream& os) const { vector_->Print(os); }

    //! Returns the number of vectors in the multi-vector.
    int num_vectors() const { return vector_->NumVectors(); }

    //! Returns the local vector length on the calling processor of vectors in the multi-vector.
    int local_length() const { return vector_->MyLength(); }

    //! Returns the global vector length of vectors in the multi-vector.
    int global_length() const { return vector_->GlobalLength(); }

    double* get_values() const { return vector_->Values(); }

    /**
     * Replace map, only if new map has same point-structure as current map.
     *
     * @warning This call may invalidate any views of this vector.
     *
     * @returns 0 if map is replaced, -1 if not.
     */
    int replace_map(const Map& map);

    int replace_local_value(int MyRow, double ScalarValue)
    {
      return vector_->ReplaceMyValue(MyRow, 0, ScalarValue);
    }

    //! Replace values in a vector with a given indexed list of values, indices are in local index
    //! space.
    int replace_local_values(int NumEntries, const double* Values, const int* Indices)
    {
      return vector_->ReplaceMyValues(NumEntries, Values, Indices);
    }

    int replace_global_value(int GlobalRow, double ScalarValue)
    {
      return vector_->ReplaceGlobalValue(GlobalRow, 0, ScalarValue);
    }

    int replace_global_values(int NumEntries, const double* Values, const int* Indices)
    {
      return vector_->ReplaceGlobalValues(NumEntries, Values, Indices);
    }

    int sum_into_local_value(int MyRow, double ScalarValue)
    {
      return vector_->SumIntoMyValue(MyRow, 0, ScalarValue);
    }

    int sum_into_global_value(int GlobalRow, double ScalarValue)
    {
      return vector_->SumIntoGlobalValue(GlobalRow, 0, ScalarValue);
    }

    int sum_into_global_values(int NumEntries, const double* Values, const int* Indices)
    {
      return vector_->SumIntoGlobalValues(NumEntries, Values, Indices);
    }

    //! Matrix-Matrix multiplication, \e this = ScalarThis*\e this + ScalarAB*A*B.
    int multiply(char TransA, char TransB, double ScalarAB, const Epetra_MultiVector& A,
        const Epetra_MultiVector& B, double ScalarThis)
    {
      return vector_->Multiply(TransA, TransB, ScalarAB, A, B, ScalarThis);
    }

    //! Multiply a Core::LinAlg::MultiVector<double> with another, element-by-element.
    int multiply(double ScalarAB, const Epetra_MultiVector& A, const Epetra_MultiVector& B,
        double ScalarThis)
    {
      return vector_->Multiply(ScalarAB, A, B, ScalarThis);
    }

    //! Puts element-wise reciprocal values of input Multi-vector in target.
    int reciprocal(const Epetra_MultiVector& A) { return vector_->Reciprocal(A); }

    int reciprocal_multiply(double ScalarAB, const Epetra_MultiVector& A,
        const Epetra_MultiVector& B, double ScalarThis)
    {
      return vector_->ReciprocalMultiply(ScalarAB, A, B, ScalarThis);
    }

    //! Imports an Epetra_DistObject using the Core::LinAlg::Import object.
    int import(const Epetra_SrcDistObject& A, const Core::LinAlg::Import& Importer,
        Epetra_CombineMode CombineMode, const Epetra_OffsetIndex* Indexor = nullptr)
    {
      return vector_->Import(A, Importer.get_epetra_import(), CombineMode, Indexor);
    }

    //! Imports an Epetra_DistObject using the Core::LinAlg::Export object.
    int import(const Epetra_SrcDistObject& A, const Core::LinAlg::Export& Exporter,
        Epetra_CombineMode CombineMode, const Epetra_OffsetIndex* Indexor = nullptr)
    {
      return vector_->Import(A, Exporter.get_epetra_export(), CombineMode, Indexor);
    }

    int export_to(const Epetra_SrcDistObject& A, const Core::LinAlg::Import& Importer,
        Epetra_CombineMode CombineMode, const Epetra_OffsetIndex* Indexor = nullptr)
    {
      return vector_->Export(A, Importer.get_epetra_import(), CombineMode, Indexor);
    }

    int export_to(const Epetra_SrcDistObject& A, const Core::LinAlg::Export& Exporter,
        Epetra_CombineMode CombineMode, const Epetra_OffsetIndex* Indexor = nullptr)
    {
      return vector_->Export(A, Exporter.get_epetra_export(), CombineMode, Indexor);
    }

    /**
     * View a given Epetra_Vector object under our own Vector wrapper.
     */
    [[nodiscard]] static std::unique_ptr<Vector<T>> create_view(Epetra_Vector& view);

    [[nodiscard]] static std::unique_ptr<const Vector<T>> create_view(const Epetra_Vector& view);

   private:
    Vector() = default;

    //! The actual Epetra_Vector object.
    Utils::OwnerOrView<Epetra_Vector> vector_;

    //! Map from Epetra_Vector
    mutable View<const Map> map_;

    //! MultiVector view of the Vector. This is used to allow implicit conversion to MultiVector.
    mutable View<MultiVector<T>> multi_vector_view_;

    friend class MultiVector<T>;
  };

  /**
   * Specialization of the Vector class for int.
   *
   * @note Currently, this specialization is mandated by a separate implementation of
   * Epetra_IntVector.
   */
  template <>
  class Vector<int>
  {
   public:
    explicit Vector(const Epetra_BlockMap& map, bool zeroOut = true);

    explicit Vector(const Map& map, bool zeroOut = true);

    Vector(const Map& map, int* values);

    Vector(const Vector& other);
    Vector& operator=(const Vector& other);
    Vector(Vector&& other) noexcept;
    Vector& operator=(Vector&& other) noexcept;

    int put_value(int Value);

    int max_value();

    int min_value();

    int& operator[](int index) { return (*vector_)[index]; }

    const int& operator[](int index) const { return (*vector_)[index]; }

    int* get_values() const { return vector_->Values(); };

    int local_length() const { return vector_->MyLength(); };

    int global_length() const { return vector_->GlobalLength(); };

    void print(std::ostream& os) const;

    //! Returns the address of the Map for this multi-vector.
    const Map& get_map() const { return map_.sync(vector_->Map()); };


    //! Imports an Epetra_DistObject using the Core::LinAlg::Import object.
    int import(const Vector& A, const Core::LinAlg::Import& Importer,
        Epetra_CombineMode CombineMode, const Epetra_OffsetIndex* Indexor = nullptr)
    {
      return vector_->Import(*A.vector_, Importer.get_epetra_import(), CombineMode, Indexor);
    }

    //! Imports an Epetra_DistObject using the Core::LinAlg::Export object.
    int import(const Vector& A, const Core::LinAlg::Export& Exporter,
        Epetra_CombineMode CombineMode, const Epetra_OffsetIndex* Indexor = nullptr)
    {
      return vector_->Import(*A.vector_, Exporter.get_epetra_export(), CombineMode, Indexor);
    }

    int export_to(const Vector& A, const Core::LinAlg::Import& Importer,
        Epetra_CombineMode CombineMode, const Epetra_OffsetIndex* Indexor = nullptr)
    {
      return vector_->Export(*A.vector_, Importer.get_epetra_import(), CombineMode, Indexor);
    }

    int export_to(const Vector& A, const Core::LinAlg::Export& Exporter,
        Epetra_CombineMode CombineMode, const Epetra_OffsetIndex* Indexor = nullptr)
    {
      return vector_->Export(*A.vector_, Exporter.get_epetra_export(), CombineMode, Indexor);
    }

    [[nodiscard]] MPI_Comm get_comm() const;

   private:
    std::shared_ptr<Epetra_IntVector> vector_;

    //! Map from Epetra_Vector
    mutable View<const Map> map_;
  };

  template <>
  struct EnableViewFor<Epetra_Vector>
  {
    using type = Vector<double>;
  };
}  // namespace Core::LinAlg

FOUR_C_NAMESPACE_CLOSE

#endif
