#include "modules/math.h"
#include "modules/file.h"
#include "modules/globals.h"

struct data
{
	int m, size;
	double *coll_energy, *sigma;
};

FILE *open_input(const int v_in,
                 const int j_in,
                 const int m_in,
                 const int v_out,
                 const int j_out,
                 const int m_out, const int J)
{
	char filename[MAX_LINE_LENGTH];

	sprintf(filename, "int_csection_iv=%d_ij=%d_im=%d_fv=%d_fj=%d_fm=%d_J=%d.dat", v_in, j_in, m_in, v_out, j_out, m_out, J);
	printf("# Using %s\n", filename);

	return file_open(filename, "r");
}

void read_csection(FILE *input, struct data *d)
{
	ASSERT(input != NULL)
	ASSERT(d->sigma == NULL);
	ASSERT(d->coll_energy == NULL);

	d->size = 0;
	char line[MAX_LINE_LENGTH];

	while (fgets(line, sizeof(line), input) != NULL)
	{
		char *token = strtok(line, " \t");

		if (token == NULL)
		{
			PRINT_ERROR("invalid entry at line '%s' (1)\n", line)
			exit(EXIT_FAILURE);
		}

		d->coll_energy = realloc(d->coll_energy, sizeof(double)*(d->size + 1));

		d->coll_energy[d->size] = atof(token);

		token = strtok(NULL, " \t");

		if (token == NULL)
		{
			PRINT_ERROR("invalid entry at line '%s' (2)\n", line)
			exit(EXIT_FAILURE);
		}

		d->sigma = realloc(d->sigma, sizeof(double)*(d->size + 1));

		d->sigma[d->size] = atof(token);
		d->size += 1;
	}

	ASSERT(d->size > 0)
}

void sum_csection(struct data *a, const struct data *b)
{
	ASSERT(a->size == b->size)
	ASSERT(a->sigma != NULL)
	ASSERT(b->sigma != NULL)
	ASSERT(a->coll_energy != NULL)
	ASSERT(b->coll_energy != NULL)

	for (int n = 0; n < a->size; ++n)
	{
		a->sigma[n] += b->sigma[n];
		ASSERT(a->coll_energy[n] == b->coll_energy[n])
	}
}
/*
struct data *read_all(const int v_in,
                      const int j_in,
                      const int v_out,
                      const int j_out,
                      const int m_out,
                      const int J_max)
{
	struct data *d = allocate(2*j_in + 1, sizeof(struct data), true);

	for (int J = 0; J <= J_max; ++J)
	{
		int counter = 0;
		for (int m = -j_in; m <= j_in; ++m)
		{
			FILE *input = open_input(v_in, j_in, m, v_out, j_out, m_out, J);

			struct data temp = {.coll_energy = NULL, .sigma = NULL};

			read_csection(input, &temp);

			if (J == 0)
			{
				d[counter].size = temp.size;
				d[counter].sigma = allocate(temp.size, sizeof(double), true);
				d[counter].coll_energy = allocate(temp.size, sizeof(double), true);

				for (int n = 0; n < temp.size; ++n)
					d[counter].coll_energy[n] = temp.coll_energy[n];
			}

			sum_csection(&d[counter], &temp);
			d[counter].m = m;

			free(temp.coll_energy);
			free(temp.sigma);

			fclose(input);
			++counter;
		}

		ASSERT((2*j_in + 1) == counter)
	}

	return d;
}*/

struct data *read_all(const int v_in,
                      const int j_in,
                      const int v_out,
                      const int j_out,
                      const int m_out,
                      const int J_max)
{
	struct data *d = allocate(2*j_in + 1, sizeof(struct data), true);

	int counter = 0;
	for (int m = -j_in; m <= j_in; ++m)
	{
		d[counter].m = m;

		FILE *input = open_input(v_in, j_in, m, v_out, j_out, m_out, 0);

		struct data temp = {.coll_energy = NULL, .sigma = NULL};

		read_csection(input, &temp);

		fclose(input);

		d[counter].size = temp.size;
		d[counter].sigma = allocate(temp.size, sizeof(double), true);
		d[counter].coll_energy = allocate(temp.size, sizeof(double), true);

		for (int n = 0; n < temp.size; ++n)
			d[counter].coll_energy[n] = temp.coll_energy[n];

		sum_csection(&d[counter], &temp);

		free(temp.coll_energy);
		free(temp.sigma);

		for (int J = 1; J <= J_max; ++J)
		{
			temp.sigma = NULL;
			temp.coll_energy = NULL;

			input = open_input(v_in, j_in, m, v_out, j_out, m_out, J);

			read_csection(input, &temp);

			fclose(input);

			sum_csection(&d[counter], &temp);

			free(temp.coll_energy);
			free(temp.sigma);
		}

		++counter;
	}

	ASSERT((2*j_in + 1) == counter)

	return d;
}

int main(int argc, char *argv[])
{
	if (argc != 9)
	{
		PRINT_ERROR("%d arguments given. Usage: %s [v, in] [j, in] [m, exp] [v, out] [j, out] [m, out] [J, max] [beta]\n", argc - 1, argv[0])
		return EXIT_FAILURE;
	}

	const int v_in = atoi(argv[1]);
	const int j_in = atoi(argv[2]);
	const int m_exp = atoi(argv[3]);
	const int v_out = atoi(argv[4]);
	const int j_out = atoi(argv[5]);
	const int m_out = atoi(argv[6]);
	const int J_max = atoi(argv[7]);
	const double beta = atof(argv[8]);

	struct data *d = read_all(v_in, j_in, v_out, j_out, m_out, J_max);

	const double a = (double) abs(m_exp);

	for (int n = 0; n < d[0].size; ++n)
	{
		printf("% -8e", d[0].coll_energy[n]*1.160451812E4);

		double sum = 0.0;
		for (int counter = 0; counter < (2*j_in + 1); ++counter)
		{
			double *wigner_d = NULL;
			const double b = (double) abs(d[counter].m);

			if (a > b)
				wigner_d = math_wigner_d(a, b, as_double(j_in), beta);
			else
				wigner_d = math_wigner_d(b, a, as_double(j_in), beta);

			sum += d[counter].sigma[n]*pow(wigner_d[j_in], 2);

			free(wigner_d);
			printf("\t % -8e", d[counter].sigma[n]/pow(1.0E-8, 2));
		}

		printf("\t % -8e\n", sum/pow(1.0E-8, 2));
	}

	free(d);
	return EXIT_SUCCESS;
}
