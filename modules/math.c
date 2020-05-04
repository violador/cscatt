/******************************************************************************

 About
 -----

 This module is a collection of special math functions for problems of AMO and
 quantum physics as well as routines to perform numerical integration, either
 using quadratures or statistical methods.

******************************************************************************/

#include "math.h"

#include <gsl/gsl_rng.h>
#include <gsl/gsl_math.h>
#include <gsl/gsl_errno.h>
#include <gsl/gsl_sf_bessel.h>
#include <gsl/gsl_monte_plain.h>
#include <gsl/gsl_monte_miser.h>
#include <gsl/gsl_monte_vegas.h>
#include <gsl/gsl_integration.h>
#include <gsl/gsl_sf_coupling.h>
#include <gsl/gsl_sf_legendre.h>

static int workspace_size = 5000;
static double abs_error = 1.0e-6;

/******************************************************************************

 Function math_legendre_poly(): returns a Legendre polynomial at x in [-1, 1].
 Where, l is positive.

******************************************************************************/

double math_legendre_poly(const int l, const double x)
{
	ASSERT(l >= 0)
	ASSERT(fabs(x) <= 1.0)

	return gsl_sf_legendre_Pl(l, x);
}

/******************************************************************************

 Function math_sphe_harmonics(): returns the spherical harmonics for angular
 momentum l and projection m at (theta, phi).

******************************************************************************/

double math_sphe_harmonics(const int l, const int m,
                           const double theta, const double phi)
{
	ASSERT(l >= abs(m))

	const double x
		= cos(theta*M_PI/180.0);

	const double m_phase
		= (m > 0? pow(-1.0, m) : 1.0);

	const double theta_wavef
		= gsl_sf_legendre_sphPlm(l, abs(m), x);

	const double phi_wavef
		= exp(as_double(m)*phi*M_PI/180.0)/sqrt(2.0*M_PI);

	/* NOTE: see equation 1.43 (pag. 8) of Angular Momentum by Richard N. Zare. */
	return m_phase*theta_wavef*phi_wavef;
}

/******************************************************************************

 Function math_wigner_3j(): returns the Wigner 3j-symbol of coupling a and b to
 result in c, where d, e and f are the projections of a, b, c, respectively.

******************************************************************************/

double math_wigner_3j(const int a, const int b, const int c,
                      const int d, const int e, const int f)
{
	return gsl_sf_coupling_3j(2*a, 2*b, 2*c, 2*d, 2*e, 2*f);
}

/******************************************************************************

 Function math_wigner_6j(): similarly to math_wigner_3j(), returns the Wigner
 6j-symbol.

******************************************************************************/

double math_wigner_6j(const int a, const int b, const int c,
                      const int d, const int e, const int f)
{
	return gsl_sf_coupling_6j(2*a, 2*b, 2*c, 2*d, 2*e, 2*f);
}

/******************************************************************************

 Function math_wigner_9j(): similarly to math_wigner_6j(), returns the Wigner
 9j-symbol.

******************************************************************************/

double math_wigner_9j(const int a, const int b, const int c,
                      const int d, const int e, const int f,
                      const int g, const int h, const int i)

{
	return gsl_sf_coupling_9j(2*a, 2*b, 2*c, 2*d, 2*e, 2*f, 2*g, 2*h, 2*i);
}

/******************************************************************************

 Function math_clebsch_gordan(): returns the Clebsch–Gordan coefficients that
 arise in the angular momentum coupling of j1 and j2 to result in j3. Where, m
 is the respective projection.

******************************************************************************/

double math_clebsch_gordan(const int j1, const int j2, const int j3,
                           const int m1, const int m2, const int m3)
{
	return pow(-1.0, j1 - j2 + m3)*sqrt(2*j3 + 1)*math_wigner_3j(j1, j2, j3, m1, m2, -m3);
}

/******************************************************************************

 Function math_set_error(): sets the absolute error for the QAG kind of methods
 (1e-6 by default). If a given abs. error cannot be reached, an error message
 is printed in the C stderr.

******************************************************************************/

void math_set_error(const double error)
{
	ASSERT(error > 0.0)
	abs_error = error;
}

/******************************************************************************

 Function math_set_workspace(): sets the size of auxiliary workspace used by
 integrators (5000 by default).

******************************************************************************/

void math_set_workspace(const int size)
{
	ASSERT(size > 0)
	workspace_size = size;
}

/******************************************************************************

 Function math_simpson(): return the integral of f(x) from a to b, using the
 1/3-Simpson quadrature rule. Where, values of f are evaluated in a grid of n
 points and params points to a struct of parameters that f may depend on (NULL
 if not needed).

******************************************************************************/

double math_simpson(const int n,
                    const double a,
                    const double b,
                    void *params,
                    const bool use_omp,
                    double (*f)(double x, void *params))
{
	ASSERT(n%2 == 0)

	const double grid_step = (b - a)/as_double(n);
	double sum = f(a, params) + f(b, params);

	#pragma omp parallel for default(none) shared(f, params) reduction(+:sum) if(use_omp)
	for (int m = 1; m < (n - 2); m += 2)
	{
		sum += 4.0*f(a + as_double(m)*grid_step, params);
		sum += 2.0*f(a + as_double(m + 1)*grid_step, params);
	}

	sum += 4.0*f(a + as_double(n - 1)*grid_step, params);

	return grid_step*sum/3.0;
}

/******************************************************************************

 Function math_2nd_simpson(): the same as math_simpson() but using Simpson's
 second rule, i.e. 3/8-Simpson quadrature.

******************************************************************************/

double math_2nd_simpson(const int n,
                        const double a,
                        const double b,
                        void *params,
                        const bool use_omp,
                        double (*f)(double x, void *params))
{
	ASSERT(n%3 == 0)

	const double grid_step = (b - a)/as_double(n);
	double sum = f(a, params) + f(b, params);

	#pragma omp parallel for default(none) shared(f, params) reduction(+:sum) if(use_omp)
	for (int m = 1; m < (n - 2); m += 3)
	{
		sum += 3.0*f(a + as_double(m)*grid_step, params);
		sum += 3.0*f(a + as_double(m + 1)*grid_step, params);
		sum += 2.0*f(a + as_double(m + 2)*grid_step, params);
	}

	sum += 3.0*f(a + as_double(n - 1)*grid_step, params);
	sum += 3.0*f(a + as_double(n - 2)*grid_step, params);

	return grid_step*3.0*sum/8.0;
}

/******************************************************************************

 Function math_qag(): return the integral of f(x) from a to b, using a 61 point
 Gauss-Kronrod rule in a QAG framework. Where, params points to a struct of
 parameters that f may depend on (NULL if not needed).

******************************************************************************/

double math_qag(const double a,
                const double b,
                void *params,
                double (*f)(double x, void *params))
{
	gsl_function gsl_f =
	{
		.params = params,
		.function = f
	};

	gsl_integration_workspace *work
		= gsl_integration_workspace_alloc(workspace_size);

	ASSERT(work != NULL)
	double result = 0.0, error = 0.0;

	const int info = gsl_integration_qag(&gsl_f, a, b, abs_error, 0.0,
	                                     workspace_size, GSL_INTEG_GAUSS61, work, &result, &error);

	gsl_integration_workspace_free(work);

	if (info != 0)
	{
		PRINT_ERROR("%s; error = %f\n", gsl_strerror(info), error)
	}

	return result;
}

/******************************************************************************

 Function math_qags(): the same as integral_qag() but using a smaller order in
 the Gauss-Kronrod rule (21) and assuming that f may be singular.

******************************************************************************/

double math_qags(const double a,
                 const double b,
                 void *params,
                 double (*f)(double x, void *params))
{
	gsl_function gsl_f =
	{
		.params = params,
		.function = f
	};

	gsl_integration_workspace *work
		= gsl_integration_workspace_alloc(workspace_size);

	ASSERT(work != NULL)
	double result = 0.0, error = 0.0;

	const int info = gsl_integration_qags(&gsl_f, a, b, abs_error, 0.0,
	                                      workspace_size, work, &result, &error);

	gsl_integration_workspace_free(work);

	if (info != 0)
	{
		PRINT_ERROR("%s; error = %f\n", gsl_strerror(info), error)
	}

	return result;
}

/******************************************************************************

 Function math_plain_mcarlo(): return the n-dimensional integral of f(x) from
 a[0, 1, ..., n] to b[0, 1, ..., n], using a plain Monte Carlo algorithm for a
 given number of f calls.

******************************************************************************/

double math_plain_mcarlo(const int n,
                         const int calls,
                         const double a[],
                         const double b[],
                         void *params,
                         double (*f)(double x[], size_t n, void *params))
{
	gsl_monte_function gsl_f =
	{
		.params = params,
		.dim = n,
		.f = f
	};

	gsl_monte_plain_state *work = gsl_monte_plain_alloc(n);
	ASSERT(work != NULL)

	gsl_rng *r = gsl_rng_alloc(gsl_rng_ranlxd2);
	ASSERT(r != NULL)

	double result = 0.0, error = 0.0;

	const int info = gsl_monte_plain_integrate(&gsl_f, a, b, n,
	                                           calls, r, work, &result, &error);

	gsl_rng_free(r);
	gsl_monte_plain_free(work);

	if (info != 0)
	{
		PRINT_ERROR("%s; error = %f\n", gsl_strerror(info), error)
	}

	return result;
}

/******************************************************************************

 Function math_vegas_mcarlo(): the same as math_plain_mcarlo() but using the
 VEGAS algorithm.

******************************************************************************/

double math_vegas_mcarlo(const int n,
                         const int calls,
                         double a[],
                         double b[],
                         void *params,
                         double (*f)(double x[], size_t n, void *params))
{
	gsl_monte_function gsl_f =
	{
		.params = params,
		.dim = n,
		.f = f
	};

	gsl_monte_vegas_state *work = gsl_monte_vegas_alloc(n);
	ASSERT(work != NULL)

	gsl_rng *r = gsl_rng_alloc(gsl_rng_ranlxd2);
	ASSERT(r != NULL)

	double result = 0.0, error = 0.0;

	const int info = gsl_monte_vegas_integrate(&gsl_f, a, b, n,
	                                           calls, r, work, &result, &error);

	gsl_rng_free(r);
	gsl_monte_vegas_free(work);

	if (info != 0)
	{
		PRINT_ERROR("%s; error = %f\n", gsl_strerror(info), error)
	}

	return result;
}

/******************************************************************************

 Function math_miser_mcarlo(): the same as math_plain_mcarlo() butusing the
 MISER algorithm.

******************************************************************************/

double math_miser_mcarlo(const int n,
                         const int calls,
                         const double a[],
                         const double b[],
                         void *params,
                         double (*f)(double x[], size_t n, void *params))
{
	gsl_monte_function gsl_f =
	{
		.params = params,
		.dim = n,
		.f = f
	};

	gsl_monte_miser_state *work = gsl_monte_miser_alloc(n);
	ASSERT(work != NULL)

	gsl_rng *r = gsl_rng_alloc(gsl_rng_ranlxd2);
	ASSERT(r != NULL)

	double result = 0.0, error = 0.0;

	const int info = gsl_monte_miser_integrate(&gsl_f, a, b, n,
	                                           calls, r, work, &result, &error);

	gsl_rng_free(r);
	gsl_monte_miser_free(work);

	if (info != 0)
	{
		PRINT_ERROR("%s; error = %f\n", gsl_strerror(info), error)
	}

	return result;
}
