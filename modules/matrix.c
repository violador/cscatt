/******************************************************************************

 About
 -----

 This module defines the opaque type matrix and many general purpose functions
 needed to manipulate it. Interfaces for few popular linear algebra libraries
 are builtin (including the use of GPUs) and are tuned during compilation.

******************************************************************************/

#if !defined(LAPACK_DISABLE_NAN_CHECK)
	#define LAPACK_DISABLE_NAN_CHECK
#endif

#if defined(USE_MKL)
	#include <mkl.h>
	#define LINEAR_ALGEBRA_LIB "Intel Math Kernel Library (MKL)"
#endif

#if defined(USE_LAPACKE)
	#include <lapacke.h>
	#include "gsl_lib.h"
	#define LINEAR_ALGEBRA_LIB "LAPACKE + GSL CBLAS"

	/* NOTE: typedef for compatibility with MKL and LAPACKE. */
	typedef enum CBLAS_TRANSPOSE CBLAS_TRANSPOSE;
#endif

#if defined(USE_MAGMA)
	#include <magma_v2.h>
	#include <magma_lapack.h>
	#define LINEAR_ALGEBRA_LIB "MAGMA"

	/* NOTE: matrix_init_gpu() will setup the queue later. */
	static magma_queue_t gpu_queue;
#endif

#if !defined(LINEAR_ALGEBRA_LIB)
	#include "gsl_lib.h"
	#define LINEAR_ALGEBRA_LIB "GNU Scientific Library (GSL)"

	/* NOTE: typedef for compatibility with MKL and LAPACKE. */
	typedef enum CBLAS_TRANSPOSE CBLAS_TRANSPOSE;
#endif

#include "matrix.h"

/******************************************************************************

 Macro DATA_OFFSET(): matrices are always ordinary vectors with elements stored
 in a row-major scheme: vector[p*max_col + q], where p = [0, max_row) and q =
 [0, max_col). Thus, DATA_OFFSET expands to the actual data offset expression
 for a given pq-element pointed by a pointer.

******************************************************************************/

struct matrix
{
	double *restrict data;
	size_t max_row, max_col, use_omp;
};

#define DATA_OFFSET(pointer, p, q) pointer->data[(p)*(pointer->max_col) + (q)]

/******************************************************************************

 Macro ASSERT_ROW_INDEX(): check if the p-th element is within the row bounds
 of a matrix pointed by a given pointer.

******************************************************************************/

#if defined(MATRIX_BOUND_CHECK)
  #define ASSERT_ROW_INDEX(pointer, p) ASSERT((p) < pointer->max_row)
#else
  #define ASSERT_ROW_INDEX(pointer, p)
#endif

/******************************************************************************

 Macro ASSERT_COL_INDEX(): check if the q-th element is within the column bounds
 of a matrix pointed by a given pointer.

******************************************************************************/

#if defined(MATRIX_BOUND_CHECK)
  #define ASSERT_COL_INDEX(pointer, q) ASSERT((q) < pointer->max_col)
#else
  #define ASSERT_COL_INDEX(pointer, q)
#endif

/******************************************************************************

 Wrapper call_dgemm(): a general call to dgemm interfacing the many different
 possible libraries. For the meaning of each input parameter, please refer to
 dgemm documentation in the netlib repository:

 http://www.netlib.org/lapack/explore-html/d7/d2b/dgemm_8f.html

 NOTE: Row-major matrices only.

******************************************************************************/

void call_dgemm(const char trans_a,
                const char trans_b,
                const int m,
                const int n,
                const int k,
                const double alpha,
                const double a[],
                const int lda,
                const double b[],
                const int ldb,
                const double beta,
                double c[],
                const int ldc)
{
	#if defined(USE_MAGMA)
	{
/*
 *		NOTE: a normal matrix in the host (row-major) implies transposed matrix
 *		in the device (col-major).
 */
		const magma_trans_t a_form
			= (trans_a == 't'? MagmaNoTrans : MagmaTrans);

		const magma_trans_t b_form
			= (trans_b == 't'? MagmaNoTrans : MagmaTrans);

		double *a_gpu = NULL, *b_gpu = NULL, *c_gpu = NULL;

		magma_dmalloc(&a_gpu, m*k);
		magma_dmalloc(&b_gpu, k*n);
		magma_dmalloc(&c_gpu, m*n);

		magma_setmatrix_async(m, k, sizeof(double), a, lda, a_gpu, lda, gpu_queue);
		magma_setmatrix_async(k, n, sizeof(double), b, ldb, b_gpu, ldb, gpu_queue);

		if (beta != 0.0)
			magma_setmatrix_async(m, n, sizeof(double), c, ldc, c_gpu, ldc, gpu_queue);

		magma_queue_sync(gpu_queue);

		magmablas_dgemm(a_form, b_form, m, n, k,
		                alpha, a_gpu, lda, b_gpu, ldb, beta, c_gpu, ldc, gpu_queue);

		magma_getmatrix(m, n, sizeof(double), c_gpu, ldc, c, ldc, gpu_queue);

		magma_free(a_gpu);
		magma_free(b_gpu);
		magma_free(c_gpu);
	}
	#else
	{
		const CBLAS_TRANSPOSE a_form
			= (trans_a == 't'? CblasTrans : CblasNoTrans);

		const CBLAS_TRANSPOSE b_form
			= (trans_b == 't'? CblasTrans : CblasNoTrans);

		cblas_dgemm(CblasRowMajor, a_form, b_form,
		            m, n, k, alpha, a, lda, b, ldb, beta, c, ldc);
	}
	#endif
}

/******************************************************************************

 Wrapper call_dsyev(): a general call to dsyev interfacing the many different
 possible libraries. For the meaning of each input parameter, please refer to
 dsyev documentation in the netlib repository:

 http://www.netlib.org/lapack/explore-html/dd/d4c/dsyev_8f.html

 NOTE: when GSL is used, only uplo = 'l' is available.

******************************************************************************/

void call_dsyev(const char jobz,
                const char uplo,
                const int n,
                double a[],
                const int lda,
                double w[])
{
	ASSERT(a != NULL)
	ASSERT(w != NULL)

	#if defined(USE_MAGMA)
	{
		const magma_vec_t job_mode = (jobz == 'n'? MagmaNoVec : MagmaVec);

		const magma_uplo_t fill_mode = (uplo == 'u'? MagmaUpper : MagmaLower);

		const magma_int_t nb = magma_get_dsytrd_nb((magma_int_t) n);

		const magma_int_t lwork
			= (jobz == 'n'? 2*n + n*nb : max(2*n + n*nb, 1 + 6*n + 2*n*n));

		const magma_int_t liwork = (jobz == 'n'? n : 3 + 5*n);

		double *a_gpu = NULL, *work = NULL;

		magma_dmalloc(&a_gpu, n*n);
		magma_dmalloc_cpu(&work, lwork);

		magma_int_t *iwork = NULL, info = 1;

		magma_imalloc_cpu(&iwork, liwork);

		magma_dsetmatrix(n, n, a, lda, a_gpu, lda, gpu_queue);

		magma_dsyevd_gpu(job_mode, fill_mode, n, a_gpu, lda,
		                 w, a, n, work, lwork, iwork, liwork, &info);

		if (info != 0)
		{
			PRINT_ERROR("magma_dsyevd_gpu() failed with error code %d\n", info)
			exit(EXIT_FAILURE);
		}

		if (jobz != 'n')
		{
			magma_dgetmatrix(n, n, a_gpu, lda, a, lda, gpu_queue);
		}

		magma_free(a_gpu);

		magma_free_cpu(work);
		magma_free_cpu(iwork);
	}
	#elif defined(USE_MKL) || defined(USE_LAPACKE)
	{
		const int info
			= LAPACKE_dsyev(LAPACK_ROW_MAJOR, jobz, uplo, n, a, lda, w);

		if (info != 0)
		{
			PRINT_ERROR("LAPACKE_dsyev() failed with error code %d\n", info)
			exit(EXIT_FAILURE);
		}
	}
	#else
	{
		ASSERT(n > 0)
		ASSERT(lda <= n)
		ASSERT(uplo == 'l')
		ASSERT(jobz == 'n' || jobz == 'v')

		gsl_matrix_view A = gsl_matrix_view_array(a, n, n);
		gsl_vector_view W = gsl_vector_view_array(w, n);

		gsl_matrix *eigenvec = gsl_matrix_alloc(n, n);

		gsl_eigen_symmv_workspace *work = gsl_eigen_symmv_alloc(n);
		gsl_eigen_symmv(&A.matrix, &W.vector, eigenvec, work);
		gsl_eigen_symmv_free(work);

		gsl_eigen_symmv_sort(&W.vector, eigenvec, GSL_EIGEN_SORT_VAL_ASC);
		if (jobz == 'v') gsl_matrix_memcpy(&A.matrix, eigenvec);

		gsl_matrix_free(eigenvec);
	}
	#endif
}

/******************************************************************************

 Function matrix_init_gpu(): allocate resources needed by MAGMA and shall be
 called before any call to functions of this module when the macro USE_MAGMA
 is defined during the compilation.

 NOTE: this function will setup properties shared by all matrix objects.

******************************************************************************/

void matrix_init_gpu()
{
	#if defined(USE_MAGMA)
	{
		if (magma_init() != MAGMA_SUCCESS)
		{
			PRINT_ERROR("%s\n", "magma_init() failed")
			exit(EXIT_FAILURE);
		}

		magma_queue_create(0, &gpu_queue);
	}
	#endif
}

/******************************************************************************

 Function matrix_end_gpu(): free resources allocated by matrix_init_gpu().

******************************************************************************/

void matrix_end_gpu()
{
	#if defined(USE_MAGMA)
	{
		if (magma_finalize() != MAGMA_SUCCESS)
		{
			PRINT_ERROR("%s\n", "magma_finalize() failed")
		}

		magma_queue_destroy(gpu_queue);
	}
	#endif
}

/******************************************************************************

 Function matrix_alloc(): allocate resources for a matrix of shape max_row-by
 -max_col.

 NOTE: this function shall return only if all input parameters are good and if
 resources are allocated properly. Thus, neither the caller or other functions
 from the module are required to (re)check returned pointers.

******************************************************************************/

matrix *matrix_alloc(const size_t max_row,
                     const size_t max_col, const bool set_zero)
{
	matrix *pointer = allocate(1, sizeof(matrix), true);

	pointer->max_row = max_row;
	pointer->max_col = max_col;

	#if defined(USE_MAGMA)
		magma_dmalloc_pinned(&pointer->data, max_row*max_col);
		if (set_zero) matrix_set_zero(pointer);
	#else
		pointer->data = allocate(max_row*max_col, sizeof(double), set_zero);
	#endif

	return pointer;
}

/******************************************************************************

 Function matrix_alloc_as(): allocate resources for a matrix with the shape of
 a given matrix m.

******************************************************************************/

matrix *matrix_alloc_as(const matrix *m, const bool set_zero)
{
	return matrix_alloc(m->max_row, m->max_col, set_zero);
}

/******************************************************************************

 Function matrix_free(): release resources allocated by matrix_alloc().

******************************************************************************/

void matrix_free(matrix *m)
{
	#if defined(USE_MAGMA)
		magma_free_pinned(m->data);
	#else
		free(m->data);
	#endif

	free(m);
}

/******************************************************************************

 Function matrix_set(): set x to the pq-element of matrix m.

******************************************************************************/

void matrix_set(matrix *m, const size_t p, const size_t q, const double x)
{
	ASSERT_ROW_INDEX(m, p)
	ASSERT_COL_INDEX(m, q)

	DATA_OFFSET(m, p, q) = x;
}

/******************************************************************************

 Function matrix_set_all(): the same as matrix_set() but for all elements.

******************************************************************************/

void matrix_set_all(matrix *m, const double x)
{
	const size_t n_max = m->max_row*m->max_col;

	#pragma omp parallel for default(none) shared(m) schedule(static) if(m->use_omp)
	for (size_t n = 0; n < n_max; ++n)
		m->data[n] = x;
}

/******************************************************************************

 Function matrix_set_diag(): set x to the p-th diagonal element of matrix m.

******************************************************************************/

void matrix_set_diag(matrix *m, const size_t p, const double x)
{
	ASSERT_ROW_INDEX(m, p)

	DATA_OFFSET(m, p, p) = x;
}

/******************************************************************************

 Function matrix_set_symm(): set x to both the pq-element and qp-element of a
 symmetric matrix m.

******************************************************************************/

void matrix_set_symm(matrix *m, const size_t p, const size_t q, const double x)
{
	ASSERT_ROW_INDEX(m, p)
	ASSERT_COL_INDEX(m, q)

	DATA_OFFSET(m, p, q) = x;
	DATA_OFFSET(m, q, p) = x;
}

/******************************************************************************

 Function matrix_set_row(): the same as matrix_set() but to set the p-th row of
 matrix m.

******************************************************************************/

void matrix_set_row(matrix *m, const size_t p, const double x)
{
	ASSERT_ROW_INDEX(m, p)

	#pragma omp parallel for default(none) shared(m) schedule(static) if(m->use_omp)
	for (size_t q = 0; q < m->max_col; ++q)
		DATA_OFFSET(m, p, q) = x;
}

/******************************************************************************

 Function matrix_set_col(): the same as matrix_set() but to set the q-th column
 of matrix m.

******************************************************************************/

void matrix_set_col(matrix *m, const size_t q, const double x)
{
	ASSERT_COL_INDEX(m, q)

	#pragma omp parallel for default(none) shared(m) schedule(static) if(m->use_omp)
	for (size_t p = 0; p < m->max_row; ++p)
		DATA_OFFSET(m, p, q) = x;
}

/******************************************************************************

 Function matrix_set_block(): the same as matrix_set() but to set a block of
 matrix m.

******************************************************************************/

void matrix_set_block(matrix *m,
                      const size_t row_min,
                      const size_t row_max,
                      const size_t col_min,
                      const size_t col_max,
                      const double x)
{
	ASSERT(row_max >= row_min)
	ASSERT(col_max >= col_min)

	ASSERT_ROW_INDEX(m, row_max)
	ASSERT_COL_INDEX(m, col_max)

	for (size_t p = row_min; p <= row_max; ++p)
		for (size_t q = col_min; q <= col_max; ++q)
			DATA_OFFSET(m, p, q) = x;
}

/******************************************************************************

 Function matrix_set_random(): the same as matrix_set_all() but all elements
 are set randomly within [0, 1].

******************************************************************************/

void matrix_set_random(matrix *m)
{
	srand(rand());
	const size_t n_max = m->max_row*m->max_col;

	#pragma omp parallel for default(none) shared(m) schedule(static) if(m->use_omp)
	for (size_t n = 0; n < n_max; ++n)
		m->data[n] = (double) rand()/RAND_MAX;
}

/******************************************************************************

 Function matrix_set_zero(): the same as matrix_set_all() but all elements are
 set to zero.

******************************************************************************/

void matrix_set_zero(matrix *m)
{
	const size_t n_max = m->max_row*m->max_col;

	#pragma omp parallel for default(none) shared(m) schedule(static) if(m->use_omp)
	for (size_t n = 0; n < n_max; ++n)
		m->data[n] = 0.0;
}

/******************************************************************************

 Function matrix_get(): return the pq-element of matrix m.

******************************************************************************/

double matrix_get(const matrix *m, const size_t p, const size_t q)
{
	ASSERT_ROW_INDEX(m, p)
	ASSERT_COL_INDEX(m, q)

	return DATA_OFFSET(m, p, q);
}

/******************************************************************************

 Function matrix_get_row(): the same as matrix_get() but return the p-th row of
 matrix m.

 NOTE: the return matrix is a row-matrix.

******************************************************************************/

matrix *matrix_get_row(const matrix *m, const size_t p)
{
	ASSERT_ROW_INDEX(m, p)

	matrix *row = matrix_alloc(1, m->max_col, false);

	#pragma omp parallel for default(none) shared(m, row) schedule(static) if(m->use_omp)
	for (size_t q = 0; q < m->max_col; ++q)
		DATA_OFFSET(row, 0, q) = DATA_OFFSET(m, p, q);

	return row;
}

/******************************************************************************

 Function matrix_get_col(): the same as matrix_get() but return the q-th column
 of matrix m.

 NOTE: the return matrix is a column-matrix.

******************************************************************************/

matrix *matrix_get_col(const matrix *m, const size_t q)
{
	ASSERT_COL_INDEX(m, q)

	matrix *col = matrix_alloc(m->max_row, 1, false);

	#pragma omp parallel for default(none) shared(m, col) schedule(static) if(m->use_omp)
	for (size_t p = 0; p < m->max_row; ++p)
		DATA_OFFSET(col, p, 0) = DATA_OFFSET(m, p, q);

	return col;
}

/******************************************************************************

 Function matrix_get_diag(): the same as matrix_get() but return the diagonal
 of matrix m.

 NOTE: the return matrix is a column-matrix.

******************************************************************************/

matrix *matrix_get_diag(const matrix *m)
{
	matrix *diag = matrix_alloc(m->max_row, 1, false);

	#pragma omp parallel for default(none) shared(m, diag) schedule(static) if(m->use_omp)
	for (size_t p = 0; p < m->max_row; ++p)
		DATA_OFFSET(diag, p, 0) = DATA_OFFSET(m, p, p);

	return diag;
}

/******************************************************************************

 Function matrix_get_block(): the same as matrix_get() but return a block of
 matrix m.

******************************************************************************/

matrix *matrix_get_block(const matrix *m,
                         const size_t row_min,
                         const size_t row_max,
                         const size_t col_min,
                         const size_t col_max)
{
	ASSERT(row_max >= row_min)
	ASSERT(col_max >= col_min)

	ASSERT_ROW_INDEX(m, row_max)
	ASSERT_COL_INDEX(m, col_max)

	matrix *block
		= matrix_alloc(row_max - row_min + 1, col_max - col_min + 1, false);

	size_t row = 0;
	for (size_t p = row_min; p <= row_max; ++p)
	{
		size_t col = 0;
		for (size_t q = col_min; q <= col_max; ++q)
		{
			DATA_OFFSET(block, row, col) = DATA_OFFSET(m, p, q);
			++col;
		}

		++row;
	}

	return block;
}

/******************************************************************************

 Function matrix_get_raw_row(): the same as matrix_get_row() but return the
 p-th row of matrix m as a C raw array.

******************************************************************************/

double *matrix_get_raw_row(const matrix *m, const size_t p)
{
	ASSERT_ROW_INDEX(m, p)

	double *row = allocate(m->max_col, sizeof(double), false);

	#pragma omp parallel for default(none) shared(m, row) schedule(static) if(m->use_omp)
	for (size_t q = 0; q < m->max_col; ++q)
		row[q] = DATA_OFFSET(m, p, q);

	return row;
}

/******************************************************************************

 Function matrix_get_raw_col(): the same as matrix_get_col() but return the
 q-th column of matrix m as a C raw array.

******************************************************************************/

double *matrix_get_raw_col(const matrix *m, const size_t q)
{
	ASSERT_COL_INDEX(m, q)

	double *col = allocate(m->max_row, sizeof(double), false);

	#pragma omp parallel for default(none) shared(m, col) schedule(static) if(m->use_omp)
	for (size_t p = 0; p < m->max_row; ++p)
		col[p] = DATA_OFFSET(m, p, q);

	return col;
}

/******************************************************************************

 Function matrix_rows(): return the number of rows in the matrix m.

******************************************************************************/

size_t matrix_rows(const matrix *m)
{
	return m->max_row;
}

/******************************************************************************

 Function matrix_cols(): return the number of columns in the matrix m.

******************************************************************************/

size_t matrix_cols(const matrix *m)
{
	return m->max_col;
}

/******************************************************************************

 Function matrix_incr(): increment the pq-element of matrix m by x (+=).

******************************************************************************/

void matrix_incr(matrix *m, const size_t p, const size_t q, const double x)
{
	ASSERT_ROW_INDEX(m, p)
	ASSERT_COL_INDEX(m, q)

	DATA_OFFSET(m, p, q) += x;
}

/******************************************************************************

 Function matrix_incr_all(): the same as matrix_incr() but for all elements.

******************************************************************************/

void matrix_incr_all(matrix *m, const double x)
{
	const size_t n_max = m->max_row*m->max_col;

	#pragma omp parallel for default(none) shared(m) schedule(static) if(m->use_omp)
	for (size_t n = 0; n < n_max; ++n)
		m->data[n] += x;
}

/******************************************************************************

 Function matrix_decr(): decrement the pq-element of matrix m by x (-=).

******************************************************************************/

void matrix_decr(matrix *m, const size_t p, const size_t q, const double x)
{
	ASSERT_ROW_INDEX(m, p)
	ASSERT_COL_INDEX(m, q)

	DATA_OFFSET(m, p, q) -= x;
}

/******************************************************************************

 Function matrix_decr_all(): the same as matrix_decr() but for all elements.

******************************************************************************/

void matrix_decr_all(matrix *m, const double x)
{
	const size_t n_max = m->max_row*m->max_col;

	#pragma omp parallel for default(none) shared(m) schedule(static) if(m->use_omp)
	for (size_t n = 0; n < n_max; ++n)
		m->data[n] -= x;
}

/******************************************************************************

 Function matrix_scale(): scale the pq-element of matrix m by x (*=).

******************************************************************************/

void matrix_scale(matrix *m, const size_t p, const size_t q, const double x)
{
	ASSERT_ROW_INDEX(m, p)
	ASSERT_COL_INDEX(m, q)

	DATA_OFFSET(m, p, q) *= x;
}

/******************************************************************************

 Function matrix_scale_all(): the same as matrix_scale() but for all elements.

******************************************************************************/

void matrix_scale_all(matrix *m, const double x)
{
	const size_t n_max = m->max_row*m->max_col;

	#pragma omp parallel for default(none) shared(m) schedule(static) if(m->use_omp)
	for (size_t n = 0; n < n_max; ++n)
		m->data[n] *= x;
}

/******************************************************************************

 Function matrix_scale_row(): the same as matrix_scale() but for the p-th row
 of matrix m.

******************************************************************************/

void matrix_scale_row(matrix *m, const size_t p, const double x)
{
	ASSERT_ROW_INDEX(m, p)

	#pragma omp parallel for default(none) shared(m) schedule(static) if(m->use_omp)
	for (size_t q = 0; q < m->max_col; ++q)
		DATA_OFFSET(m, p, q) *= x;
}

/******************************************************************************

 Function matrix_scale_col(): the same as matrix_scale() but for the q-th
 column of matrix m.

******************************************************************************/

void matrix_scale_col(matrix *m, const size_t q, const double x)
{
	ASSERT_COL_INDEX(m, q)

	#pragma omp parallel for default(none) shared(m) schedule(static) if(m->use_omp)
	for (size_t p = 0; p < m->max_row; ++p)
		DATA_OFFSET(m, p, q) *= x;
}

/******************************************************************************

 Function matrix_copy_element(): copy the lk-element of matrix b to the
 pq-element of matrix a.

******************************************************************************/

void matrix_copy_element(matrix *a,
                         const size_t p,
                         const size_t q,
                         const matrix *b,
                         const size_t l,
                         const size_t k)
{
	ASSERT_ROW_INDEX(a, p)
	ASSERT_COL_INDEX(a, q)

	ASSERT_ROW_INDEX(b, l)
	ASSERT_COL_INDEX(b, k)

	DATA_OFFSET(a, p, q) = DATA_OFFSET(b, l, k);
}

/******************************************************************************

 Function matrix_copy(): copy all elements from b to a, a = b*alpha + beta.

******************************************************************************/

void matrix_copy(matrix *a,
                 const matrix *b, const double alpha, const double beta)
{
	const size_t n_max
		= min(a->max_row*a->max_col, b->max_row*b->max_col);

	#pragma omp parallel for default(none) shared(a, b) schedule(static) if(a->use_omp)
	for (size_t n = 0; n < n_max; ++n)
		a->data[n] = b->data[n]*alpha + beta;
}

/******************************************************************************

 Function matrix_swap(): swap the shape and elements of matrices a and b.

******************************************************************************/

void matrix_swap(matrix *a, matrix *b)
{
	const size_t b_row = b->max_row;
	const size_t b_col = b->max_col;
	double *b_data = b->data;

	b->max_row = a->max_row;
	b->max_col = a->max_col;
	b->data = a->data;

	a->max_row = b_row;
	a->max_col = b_col;
	a->data = b_data;
}

/******************************************************************************

 Function matrix_trace(): return the trace of (a square) matrix m, tr(m).

******************************************************************************/

double matrix_trace(const matrix *m)
{
	double sum = 0.0;

	#pragma omp parallel for default(none) shared(m) reduction(+:sum) if(m->use_omp)
	for (size_t n = 0; n < m->max_row; ++n)
		sum += DATA_OFFSET(m, n, n);

	return sum;
}

/******************************************************************************

 Function matrix_sum(): return the sum of all elements of matrix m.

******************************************************************************/

double matrix_sum(const matrix *m)
{
	double sum = 0.0;
	const size_t n_max = m->max_row*m->max_col;

	#pragma omp parallel for default(none) shared(m) reduction(+:sum) if(m->use_omp)
	for (size_t n = 0; n < n_max; ++n)
		sum += m->data[n];

	return sum;
}

/******************************************************************************

 Function matrix_sum_row(): return the sum of all elements in the row p of
 matrix m.

******************************************************************************/

double matrix_sum_row(const matrix *m, const size_t p)
{
	ASSERT_ROW_INDEX(m, p)

	double sum = 0.0;

	#pragma omp parallel for default(none) shared(m) reduction(+:sum) if(m->use_omp)
	for (size_t q = 0; q < m->max_col; ++q)
		sum += DATA_OFFSET(m, p, q);

	return sum;
}

/******************************************************************************

 Function matrix_sum_col(): return the sum of all elements in the column q of
 matrix m.

******************************************************************************/

double matrix_sum_col(const matrix *m, const size_t q)
{
	ASSERT_COL_INDEX(m, q)

	double sum = 0.0;

	#pragma omp parallel for default(none) shared(m) reduction(+:sum) if(m->use_omp)
	for (size_t p = 0; p < m->max_row; ++p)
		sum += DATA_OFFSET(m, p, q);

	return sum;
}

/******************************************************************************

 Function matrix_min(): return the smallest element of matrix m.

******************************************************************************/

double matrix_min(const matrix *m)
{
	double min = INF;
	const size_t n_max = m->max_row*m->max_col;

	for (size_t n = 0; n < n_max; ++n)
		if (m->data[n] < min) min = m->data[n];

	return min;
}

/******************************************************************************

 Function matrix_max(): return the biggest element of matrix m.

******************************************************************************/

double matrix_max(const matrix *m)
{
	double max = -INF;
	const size_t n_max = m->max_row*m->max_col;

	for (size_t n = 0; n < n_max; ++n)
		if (m->data[n] > max) max = m->data[n];

	return max;
}

/******************************************************************************

 Function matrix_multiply(): perform the operation c = alpha*a*b + beta*c.

******************************************************************************/

void matrix_multiply(const double alpha,
                     const matrix *a, const matrix *b, const double beta, matrix *c)
{
	call_dgemm('n', 'n', a->max_row, b->max_col, a->max_col, alpha, a->data,
	            a->max_row, b->data, a->max_col, beta, c->data, c->max_row);
}

/******************************************************************************

 Function matrix_add(): perform the operation c = a*alpha + b*beta.

******************************************************************************/

void matrix_add(const double alpha, const matrix *a,
                const double beta, const matrix *b, matrix *c)
{
	const size_t n_max
		= min(a->max_row*a->max_col, b->max_row*b->max_col);

	#pragma omp parallel for default(none) shared(a, b, c) schedule(static) if(c->use_omp)
	for (size_t n = 0; n < n_max; ++n)
		c->data[n] = a->data[n]*alpha + b->data[n]*beta;
}

/******************************************************************************

 Function matrix_sub(): perform the operation c = alpha*a - beta*b.

******************************************************************************/

void matrix_sub(const double alpha, const matrix *a,
                const double beta, const matrix *b, matrix *c)
{
	const size_t n_max
		= min(a->max_row*a->max_col, b->max_row*b->max_col);

	#pragma omp parallel for default(none) shared(a, b, c) schedule(static) if(c->use_omp)
	for (size_t n = 0; n < n_max; ++n)
		c->data[n] = a->data[n]*alpha - b->data[n]*beta;
}

/******************************************************************************

 Function matrix_inverse(): to invert the matrix m.

******************************************************************************/

void matrix_inverse(matrix *m)
{
	#if defined(USE_MAGMA)
	{
		magma_int_t *ipiv = NULL;
		magma_imalloc_cpu(&ipiv, min(m->max_row, m->max_col));

		double *m_gpu = NULL;
		magma_dmalloc(&m_gpu, m->max_row*m->max_col);

		magma_setmatrix(m->max_row, m->max_col, sizeof(double),
		                m->data, m->max_row, m_gpu, m->max_row, gpu_queue);

		magma_int_t info = 1;
		magma_dgetrf_gpu(m->max_row, m->max_col, m_gpu, m->max_row, ipiv, &info);

		if (info != 0)
		{
			PRINT_ERROR("magma_dgetrf_gpu() failed with error code %d\n", info)
			exit(EXIT_FAILURE);
		}

		const magma_int_t lwork
			= m->max_row*magma_get_dgetri_nb(m->max_row);

		double *work = NULL;
		magma_dmalloc(&work, lwork);

		info = 1;
		magma_dgetri_gpu(m->max_row, m_gpu, m->max_row, ipiv, work, lwork, &info);

		if (info != 0)
		{
			PRINT_ERROR("magma_dgetri_gpu() failed with error code %d\n", info)
			exit(EXIT_FAILURE);
		}

		magma_getmatrix(m->max_row, m->max_col, sizeof(double),
		                m_gpu, m->max_row, m->data, m->max_row, gpu_queue);

		magma_free(ipiv);
		magma_free(work);
		magma_free(m_gpu);
	}
	#elif defined(USE_MKL) || defined(USE_LAPACKE)
	{
		#if defined(USE_LAPACKE)
			int *ipiv = malloc(sizeof(int)*m->max_row);
		#else
			long long int *ipiv = malloc(sizeof(long long int)*m->max_row);
		#endif

		ASSERT(ipiv != NULL)

		int info = LAPACKE_dsytrf(LAPACK_ROW_MAJOR, 'u', m->max_row,
		                                 m->data, m->max_row, ipiv);

		if (info != 0)
		{
			PRINT_ERROR("LAPACKE_dsytrf() failed with error code %d\n", info)
			exit(EXIT_FAILURE);
		}

		info = LAPACKE_dsytri(LAPACK_ROW_MAJOR, 'u', m->max_row,
		                             m->data, m->max_row, ipiv);

		if (info != 0)
		{
			PRINT_ERROR("LAPACKE_dsytri() failed with error code %d\n", info)
			exit(EXIT_FAILURE);
		}

/*
 *		NOTE: To this point, only the upper triangular part is actually
 *		inverted. Thus, we have to copy lapacke result into the lower one.
 */

		for (size_t p = 0; p < m->max_row; ++p)
			for (size_t q = (p + 1); q < m->max_col; ++q)
				DATA_OFFSET(m, q, p) = DATA_OFFSET(m, p, q);

		free(ipiv);
	}
	#else
	{
		double *buffer = allocate(m->max_row*m->max_col, sizeof(double), false);

		for (size_t n = 0; n < m->max_row*m->max_col; ++n)
			buffer[n] = m->data[n];

		gsl_permutation *p = gsl_permutation_alloc(m->max_row);
		gsl_matrix_view  a = gsl_matrix_view_array(buffer, m->max_row, m->max_col);
		gsl_matrix_view  b = gsl_matrix_view_array(m->data, m->max_row, m->max_col);

		int sign = 0;
		gsl_linalg_LU_decomp(&a.matrix, p, &sign);
		gsl_linalg_LU_invert(&a.matrix, p, &b.matrix);

		gsl_permutation_free(p);
		free(buffer);
	}
	#endif
}

/******************************************************************************

 Function matrix_symm_eigen(): return the eigenvalues of a symmetric matrix. On
 exit, the original matrix is replaced by the respective eigenvectors if job =
 'v'.

******************************************************************************/

double *matrix_symm_eigen(matrix *m, const char job)
{
	double *eigenval = allocate(m->max_row, sizeof(double), false);

	call_dsyev(job, 'l', m->max_row, m->data, m->max_row, eigenval);

	return eigenval;
}

/******************************************************************************

 Function matrix_is_null(): return true if all elements are zero. Return false
 otherwise.

******************************************************************************/

bool matrix_is_null(const matrix *m)
{
	const size_t n_max = m->max_row*m->max_col;

	for (size_t n = 0; n < n_max; ++n)
		if (m->data[n] != 0.0) return false;

	return true;
}

/******************************************************************************

 Function matrix_is_positive(): return true if all elements are positive.
 Return false otherwise.

******************************************************************************/

bool matrix_is_positive(const matrix *m)
{
	const size_t n_max = m->max_row*m->max_col;

	for (size_t n = 0; n < n_max; ++n)
		if (m->data[n] < 0.0) return false;

	return true;
}

/******************************************************************************

 Function matrix_is_negative(): return true if all elements are negative.
 Return false otherwise.

******************************************************************************/

bool matrix_is_negative(const matrix *m)
{
	const size_t n_max = m->max_row*m->max_col;

	for (size_t n = 0; n < n_max; ++n)
		if (m->data[n] >= 0.0) return false;

	return true;
}

/******************************************************************************

 Function matrix_is_square(): return true if m is a square matrix. Return false
 otherwise.

******************************************************************************/

bool matrix_is_square(const matrix *m)
{
	return (m->max_row == m->max_col);
}

/******************************************************************************

 Function matrix_has_nan(): return true if at least one element has a NaN.
 Return false otherwise.

******************************************************************************/

bool matrix_has_nan(const matrix *m)
{
	const size_t n_max = m->max_row*m->max_col;

	for (size_t n = 0; n < n_max; ++n)
		if (isnan(m->data[n]) != 0) return true;

	return false;
}

/******************************************************************************

 Function matrix_using_magma(): return true if the macro USE_MAGMA was used
 during compilation, false otherwise.

******************************************************************************/

bool matrix_using_magma()
{
	#if defined(USE_MAGMA)
		return true;
	#else
		return false;
	#endif
}

/******************************************************************************

 Function matrix_using_mkl(): return true if the macro USE_MKL was used during
 compilation, false otherwise.

******************************************************************************/

bool matrix_using_mkl()
{
	#if defined(USE_MKL)
		return true;
	#else
		return false;
	#endif
}

/******************************************************************************

 Function matrix_using_lapacke(): return true if the macro USE_LAPACKE was used
 during compilation, false otherwise.

******************************************************************************/

bool matrix_using_lapacke()
{
	#if defined(USE_LAPACKE)
		return true;
	#else
		return false;
	#endif
}

/*****************************************************************************

 Function matrix_save(): saves a matrix object in the disk. Binary format is
 used.

******************************************************************************/

void matrix_save(const matrix *m, const char filename[])
{
	FILE *output = fopen(filename, "wb");

	if (output == NULL)
	{
		PRINT_ERROR("unable to open %s\n", filename)
		exit(EXIT_FAILURE);
	}

	size_t info = 0;

	info = fwrite(&m->max_row, sizeof(size_t), 1, output);
	ASSERT(info == 1)

	info = fwrite(&m->max_col, sizeof(size_t), 1, output);
	ASSERT(info == 1)

	info = fwrite(m->data, sizeof(double), m->max_row*m->max_col, output);
	ASSERT(info == m->max_row*m->max_col)

	fclose(output);
}

/******************************************************************************

 Function matrix_load(): load a matrix object from the disk which has been
 written by matrix_save().

******************************************************************************/

matrix *matrix_load(const char filename[])
{
	FILE *input = fopen(filename, "rb");

	if (input == NULL)
	{
		PRINT_ERROR("unable to open %s\n", filename)
		exit(EXIT_FAILURE);
	}

	size_t info = 0, max_row = 0, max_col = 0;

	info = fread(&max_row, sizeof(size_t), 1, input);
	ASSERT(info == 1)

	info = fread(&max_col, sizeof(size_t), 1, input);
	ASSERT(info == 1)

	matrix *m = matrix_alloc(max_row, max_col, false);

	info = fread(m->data, sizeof(double), m->max_row*m->max_col, input);
	ASSERT(info == m->max_row*m->max_col)

	fclose(input);
	return m;
}

/******************************************************************************

 Function matrix_read(): reads max_row lines and max_col columns from a data
 file and return them as a matrix.

 NOTE: column data separated either by spaces or tabs are expected.

******************************************************************************/

matrix *matrix_read(FILE *input, const size_t max_row, const size_t max_col)
{
	ASSERT(input != NULL)

	matrix *m = matrix_alloc(max_row, max_col, false);

	rewind(input);
	char line[MAX_LINE_LENGTH] = "\n";

	size_t p = 0;
	while (p < max_row)
	{
		if (fgets(line, sizeof(line), input) == NULL) break;

		if ((line[0] != '#') && (line[0] != '\n') && (line[0] != '\0'))
		{
			char *token = strtok(line, " \t");

			for (size_t q = 0; q < max_col; ++q)
			{
				if (token != NULL) DATA_OFFSET(m, p, q) = atof(token);
				token = strtok(NULL, " \t");
			}

			++p;
		}
	}

	return m;
}

/******************************************************************************

 Function matrix_write(): writes to a data file max_row lines and max_col
 columns of a matrix m.

******************************************************************************/

void matrix_write(const matrix *m,
                  FILE *output, const size_t max_row, const size_t max_col)
{
	ASSERT(output != NULL)

	const size_t p_max = min(max_row, m->max_row);
	const size_t q_max = min(max_col, m->max_col);

	for (size_t p = 0; p < p_max; ++p)
	{
		for (size_t q = 0; q < q_max; ++q)
		{
/*
 *			NOTE: "% -8e\t" print numbers left-justified with an invisible
 *			plus sign, if any, 8 digits wide in scientific notation + tab.
 */
			fprintf(output, "% -8e\t", DATA_OFFSET(m, p, q));
		}

		fprintf(output, "\n");
	}
}

/******************************************************************************

 Function matrix_sizeof(): return the total size in bytes of the content hold
 by matrix m.

******************************************************************************/

size_t matrix_sizeof(const matrix *m)
{
	return 3*sizeof(size_t) + m->max_row*m->max_col*sizeof(double);
}

/******************************************************************************

 Function matrix_use_omp(): turn on/off the use of the OpenMP libray.

******************************************************************************/

void matrix_use_omp(matrix *m, const bool use)
{
	m->use_omp = (size_t) use;
}

/******************************************************************************

 Function matrix_reshape(): resize the shape of a matrix m after initialized by
 matrix_alloc().

******************************************************************************/

void matrix_reshape(matrix *m, const size_t max_row, const size_t max_col)
{
	#if defined(USE_MAGMA)
		PRINT_ERROR("%s\n", "memory pinned reallocation not available when using MAGMA")
		exit(EXIT_FAILURE);
	#endif

	m->max_row = max_row;
	m->max_col = max_col;
	m->data = realloc(m->data, sizeof(double)*m->max_row*m->max_col);

	ASSERT(m->data != NULL)
}

/******************************************************************************

 Function matrix_data_set(): set x to the n-th element in the internal array
 that stores the matrix m, whose physical length in memory is max_row*max_col.

******************************************************************************/

void matrix_data_set(matrix *m, const size_t n, const double x)
{
	m->data[n] = x;
}

/******************************************************************************

 Function matrix_data_get(): accesses the n-th element in the internal array
 that stores the matrix m, whose physical length in memory is max_row*max_col.

******************************************************************************/

double matrix_data_get(const matrix *m, const size_t n)
{
	return m->data[n];
}

/******************************************************************************

 Function matrix_data_raw(): returns the whole internal array that stores the
 matrix m, whose physical length in memory is max_row*max_col.

******************************************************************************/

double *matrix_data_raw(const matrix *m)
{
	const size_t n_max = m->max_row*m->max_col;

	double *data = allocate(n_max, sizeof(double), false);

	#pragma omp parallel for default(none) shared(m, data) schedule(static) if(m->use_omp)
	for (size_t n = 0; n < n_max; ++n)
		data[n] = m->data[n];

	return data;
}

/******************************************************************************

 Function matrix_data_length(): returns the internal physical length of m as it
 is stored in memory, max_row*max_col.

******************************************************************************/

size_t matrix_data_length(const matrix *m)
{
	return m->max_row*m->max_col;
}

/******************************************************************************

 Function matrix_about(): prints in a given output file the conditions in which
 the module was compiled.

******************************************************************************/

void matrix_about(FILE *output)
{
	ASSERT(output != NULL)

	fprintf(output, "# build date   = %s\n", __DATE__);
	fprintf(output, "# source code  = %s\n", __FILE__);
	fprintf(output, "# data layout  = %s\n", "row-major scheme");
	fprintf(output, "# lin. algebra = %s\n", LINEAR_ALGEBRA_LIB);
}
