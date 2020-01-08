#include "config.h"

#include "modules/pes.h"
#include "modules/dvr.h"
#include "modules/mass.h"
#include "modules/file.h"
#include "modules/matrix.h"
#include "modules/globals.h"

#define FORMAT1 "# %5d   %5d   %5d   %5d     % -8e  % -8e  % -8e\n"
#define FORMAT2 "# %5d   %5d   %5d   %5d\n"

int main(int argc, char *argv[])
{
	ASSERT(argc > 1)
	file_init_stdin(argv[1]);

/*
 *	Total angular momentum, J:
 */

	const int J
		= (int) file_get_key(stdin, "J", 0.0, INF, 0.0);

	const int J_parity
		= (int) file_get_key(stdin, "parity", -1.0, 1.0, 0.0);

/*
 *	Vibrational quantum numbers, v:
 */

	const int v_min
		= (int) file_get_key(stdin, "v_min", 0.0, INF, 0.0);

	const int v_max
		= (int) file_get_key(stdin, "v_max", as_double(v_min), INF, as_double(v_min));

	const int v_step
		= (int) file_get_key(stdin, "v_step", 1.0, INF, 1.0);

/*
 *	Rotational quantum numbers, j:
 */

	const int j_min
		= (int) file_get_key(stdin, "j_min", 0.0, INF, 0.0);

	const int j_max
		= (int) file_get_key(stdin, "j_max", as_double(j_min), INF, as_double(j_min));

	const int j_step
		= (int) file_get_key(stdin, "j_step", 1.0, INF, 1.0);

/*
 *	Vibrational grid:
 */

	const int rovib_grid_size
		= (int) file_get_key(stdin, "rovib_grid_size", as_double(v_max + 1), INF, 500.0);

	const double r_min
		= file_get_key(stdin, "r_min", 0.0, INF, 0.0);

	const double r_max
		= file_get_key(stdin, "r_max", r_min, INF, r_min + 100.0);

	const double r_step
		= (r_max - r_min)/as_double(rovib_grid_size);

/*
 *	Electronic spin multiplicity:
 */

	const int spin_mult
		= (int) file_get_key(stdin, "spin_mult", 1.0, 3.0, 1.0);

/*
 *	Arrangement (1 == a, 2 == b, 3 == c) and atomic masses:
 */

	const char arrang
		= 96 + (int) file_get_key(stdin, "arrangement", 1.0, 3.0, 1.0);

	mass_init(stdin);

	printf("#\n");
	printf("# REDUCED MASSES:\n");
	printf("# Atom A      = %f a.u.\n", mass(atom_a));
	printf("# Atom B      = %f a.u.\n", mass(atom_b));
	printf("# Atom C      = %f a.u.\n", mass(atom_c));

	enum mass_case m;

	switch (arrang)
	{
		case 'a':
			m = pair_bc;
			printf("# Diatom BC   = %f a.u.\n", mass(m));
			printf("# Arrangement = A + BC\n");
		break;

		case 'b':
			m = pair_ac;
			printf("# Diatom CA   = %f a.u.\n", mass(m));
			printf("# Arrangement = B + CA\n");
		break;

		case 'c':
			m = pair_ab;
			printf("# Diatom AB   = %f a.u.\n", mass(m));
			printf("# Arrangement = C + AB\n");
		break;

		default:
			m = pair_bc;
			printf("# Diatom BC   = %f a.u.\n", mass(m));
			printf("# Arrangement = A + BC (assuming arrang = 1)\n");
	}

/*
 *	Resolve the diatomic eigenvalue for each j-case and sort results as scatt. channels:
 */

	printf("#\n");
	printf("# J = %d\n", J);
	printf("#   Ch.       v       j       l        E (a.u.)       E (cm-1)         E (eV)  \n");
	printf("# -----------------------------------------------------------------------------\n");

/*
 *	Step 1: loop over rotational states j of the diatom and solve the diatomic eigenvalue problem
 *	using the Fourier grid Hamiltonian discrete variable representation (FGH-DVR) method.
 */

	int max_ch = 0;

	for (int j = j_min; j <= j_max; j += j_step)
	{
		double *pot_energy = allocate(rovib_grid_size, false);

		for (int n = 0; n < rovib_grid_size; ++n)
		{
			const double r = r_min + as_double(n)*r_step;
			pot_energy[n] = pec(arrang, r) + centr_term(j, mass(m), r);
		}

		matrix *eigenvec
			= dvr_fgh(rovib_grid_size, r_step, pot_energy, mass(m));

		free(pot_energy);

		double *eigenval
			= matrix_symm_eigen(eigenvec, 'v');

/*
 *		Step 2: loop over the vibrational states v of the diatom, solutions of step 2, and select
 *		only those of interest.
 */

		for (int v = v_min; v <= v_max; v += v_step)
		{
			dvr_fgh_norm(eigenvec, v, r_step, false);
			matrix *wavef = matrix_get_col(eigenvec, v, false);

/*
 *			Step 3: loop over all partial waves l of the atom around the diatom given by the
 *			respective J and j.
 */

			for (int l = abs(J - j); l <= (J + j); ++l)
			{
				if (parity(j + l) != J_parity && J_parity != 0) continue;

				if (l == abs(J - j))
					printf(FORMAT1, max_ch, v, j, l, eigenval[v], eigenval[v]*219474.63137054, eigenval[v]*27.211385);
				else
					printf(FORMAT2, max_ch, v, j, l);

/*
 *				Step 4: save each basis function |vjl> in the disk and increment the counter of
 *				channels.
 */

				char filename[MAX_LINE_LENGTH];
				sprintf(filename, BASIS_BUFFER_FORMAT, arrang, max_ch, J);

				matrix_save(wavef, filename);

				file_write(filename, 1, sizeof(double), &r_min, true);
				file_write(filename, 1, sizeof(double), &r_max, true);
				file_write(filename, 1, sizeof(double), &r_step, true);
				file_write(filename, 1, sizeof(double), &eigenval[v], true);

				file_write(filename, 1, sizeof(int), &spin_mult, true);
				file_write(filename, 1, sizeof(int), &l, true);
				file_write(filename, 1, sizeof(int), &j, true);
				file_write(filename, 1, sizeof(int), &v, true);

				++max_ch;
			}

			matrix_free(wavef);
		}

		matrix_free(eigenvec);
		free(eigenval);
	}

	printf("\n# A total of %d basis functions are computed with %d grid "
	       "points in r = [%f, %f)\n", max_ch, rovib_grid_size, r_min, r_max);

	return EXIT_SUCCESS;
}