#include "modules/fgh.h"
#include "modules/file.h"
#include "modules/matrix.h"
#include "modules/globals.h"

int main(int argc, char *argv[])
{
	ASSERT(argc > 1)

	file_init_stdin(argv[1]);

/*
 *	Total angular momentum, J:
 */

	const int J_min
		= read_int_keyword(stdin, "J_min", 0, 10000, 0);

	const int J_max
		= read_int_keyword(stdin, "J_max", J_min, 10000, J_min);

	const int J_step
		= read_int_keyword(stdin, "J_step", 1, 10000, 1);

/*
 *	Arrangement (a = 1, b = 2, c = 3):
 */

	const char arrang
		= 96 + read_int_keyword(stdin, "arrang", 1, 3, 1);

/*
 *	Directory to load all basis functions from:
 */

	char *dir = read_str_keyword(stdin, "basis_dir", ".");

	if (!file_exist(dir) && dir[0] != '.')
	{
		PRINT_ERROR("%s does not exist\n", dir)
		exit(EXIT_FAILURE);
	}

/*
 *	Print the basis functions for each J:
 */

	for (int J = J_min; J <= J_max; J += J_step)
	{
		const int max_channel = fgh_basis_count(dir, arrang, J);

		for (int ch = 0; ch < max_channel; ++ch)
		{
			fgh_basis b;

			FILE *input = fgh_basis_file(dir, arrang, ch, J, "rb", true);

			fgh_basis_read(&b, input);

			file_close(&input);

			FILE *output = fgh_basis_file(".", arrang, ch, J, "w", true);

			ASSERT(output != NULL)

			fprintf(output, "# v = %d\n", b.v);
			fprintf(output, "# j = %d\n", b.j);
			fprintf(output, "# l = %d\n", b.l);
			fprintf(output, "# Component  = %d\n", b.n);
			fprintf(output, "# Eigenvalue = % -8e\n", b.eigenval);
			fprintf(output, "# File created at %s\n", time_stamp());

			for (int n = 0; n < b.grid_size; ++n)
			{
				const double r = b.r_min + as_double(n)*b.r_step;
/*
 *				NOTE: "% -8e\t" print numbers left-justified with an invisible
 *				plus sign, if any, 8 digits wide in scientific notation + tab.
 */
				fprintf(output, "%06f\t % -8e\t\n", r, b.eigenvec[n]);
			}

			file_close(&output);
			free(b.eigenvec);
		}
	}

	free(dir);
	return EXIT_SUCCESS;
}
