#if !defined(MPI_LIB_HEADER)
	#define MPI_LIB_HEADER
	#include "c_lib.h"

	typedef struct mpi_matrix mpi_matrix;

	typedef struct mpi_vector mpi_vector;

	void mpi_init(int argc, char *argv[]);

	bool mpi_using_petsc();

	bool mpi_using_slepc();

	void mpi_end();

	size_t mpi_rank();

	size_t mpi_comm_size();

	size_t mpi_thread_level();

	void mpi_barrier();

	void mpi_set_tasks(const size_t max_task);

	size_t mpi_first_task();

	size_t mpi_last_task();

	size_t mpi_extra_task();

	bool mpi_inbox(const size_t from);

	void mpi_send(const size_t to, const size_t n, const char type, void *data);

	void mpi_receive(const size_t from, const size_t n, const char type, void *data);

	mpi_matrix *mpi_matrix_alloc(const size_t max_row,
	                             const size_t max_col, const int non_zeros[]);

	void mpi_matrix_free(mpi_matrix *m);

	void mpi_matrix_set(mpi_matrix *m,
	                    const int p, const int q, const double x);

	void mpi_matrix_build(mpi_matrix *m);

	mpi_vector *mpi_vector_alloc(const int length);

	void mpi_vector_free(mpi_vector *v);

	void mpi_vector_build(mpi_vector *v);

	int mpi_matrix_sparse_eigen(mpi_matrix *m, const int n,
	                            const int max_step, const double tol, const bool up);

	mpi_vector *mpi_matrix_eigenpair(const mpi_matrix *m,
	                                 const int n, double *eigenval);

	void mpi_vector_write(const mpi_vector *v,
	                      const int n_min, const int n_max, FILE *stream);

	void mpi_about(FILE *output);

	/******************************************************************************

	 Macro MPI_PRINTF(): prints a formatted message from a given process (master
	 thread) in the C stdout of process 0 (master thread).

	 NOTE: a maximum of 1024 characters only.

	******************************************************************************/

	#define MPI_PRINTF(format, ...)         \
	{                                       \
	  char line[1024];                      \
	  sprintf(line, format, ##__VA_ARGS__); \
	                                        \
	  mpi_printf(line);                     \
	}
#endif
