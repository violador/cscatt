#if !defined(PES_HEADER)
	#define PES_HEADER
	#include "globals.h"
	#include "matrix.h"

	#define PES_MAX_JACOBI_VECTOR 5
	#define PES_MAX_ATOM (PES_MAX_JACOBI_VECTOR + 1)
	#define PES_MAX_INTERNUC_DISTANCE (3*PES_MAX_ATOM - 6)

	struct pes_coor
	{
		char arrang;
		size_t lambda;
		double r[PES_MAX_JACOBI_VECTOR], R, theta, phi;
	};

	typedef struct pes_coor pes_coor;

	/******************************************************************************

	 Type pes_multipole_set: value[lambda][n], n from r1

	******************************************************************************/

	struct pes_multipole
	{
		double R, r_min, r_max, r_step, **value;
		size_t lambda_min, lambda_max, lambda_step, grid_size;
	};

	typedef struct pes_multipole pes_multipole;

	typedef struct pes_multipole_set pes_multipole_set;

	void pes_set_inf(const double x_inf);

	void pes_init_mass(FILE *input, const char atom);

	void pes_init();

	double pes_mass(const char atom);

	double pes_mass_bc();

	double pes_mass_ac();

	double pes_mass_ab();

	double pes_mass_abc(const char arrang);

	double pes_mass_abcd();

	const char *pes_name();

	double pes_abc(const char arrang,
	               const double r, const double R, const double theta);

	double pes_abcd(const double r[],
	                const double R, const double theta, const double phi);

	double pes_bc(const size_t j, const double r);

	double pes_ac(const size_t j, const double r);

	double pes_ab(const size_t j, const double r);

	double pes_legendre_multipole(const char arrang, const size_t lambda,
	                              const double r, const double R);

	double pes_harmonic_multipole(const size_t lambda,
	                              const size_t m, const double r[], const double R);

	FILE *pes_multipole_file(const char dir[], const char arrang,
	                         const size_t n, const char mode[], const bool verbose);

	void pes_multipole_init(pes_multipole *m);

	void pes_multipole_init_all(const size_t n_max, pes_multipole m[]);

	void pes_multipole_write(const pes_multipole *m, FILE *output);

	void pes_multipole_write_all(const size_t n_max,
	                             const pes_multipole m[], FILE *output);

	void pes_multipole_read(pes_multipole *m, FILE *input);

	void pes_multipole_read_all(const size_t n_max, pes_multipole m[], FILE *input);

	size_t pes_multipole_count(const char dir[], const char arrang);

	void pes_multipole_save(const pes_multipole *m,
	                        const char dir[], const char arrang, const size_t n);

	void pes_multipole_load(pes_multipole *m,
	                        const char dir[], const char arrang, const size_t n);

	void pes_multipole_free(pes_multipole *m);

	double pes_olson_smith_model(const size_t n, const size_t m, const double x);

	double pes_tully_1st_model(const size_t n, const size_t m, const double x);

	double pes_tully_2nd_model(const size_t n, const size_t m, const double x);

	double pes_tully_3rd_model(const size_t n, const size_t m, const double x);

	void pes_about(FILE *output);
#endif
