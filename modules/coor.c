#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "globals.h"
#include "coor.h"
#include "mass.h"

/******************************************************************************

 Function coor_jacobi_to_internuc(): converts from a set of Jacobi coordinates
 to a set of internuclear distances for triatomic systems. The following order
 of arrangements is used: arrangement index 'a' for A + BC, 'b' for B + CA and
 'c' is C + AB. Where, A, B and C represents three atoms.

******************************************************************************/

void coor_jacobi_to_internuc(const jacobi_coor *from, internuc_coor *to)
{
	xyz_coor a, b, c;

	switch (from->arrang)
	{
		case 'a':
			c.x = 0.0;
			c.y = from->r/2.0;
			c.z = 0.0;

			b.x =  0.0;
			b.y = -c.y;
			b.z =  0.0;

			a.x = 0.0;

			a.y = (c.y*mass(atom_c) + b.y*mass(atom_b))/(mass(atom_c) + mass(atom_b))
			    + from->R*sin(from->theta*M_PI/180.0);

			a.z = from->R*cos(from->theta*M_PI/180.0);

			to->r_bc = from->r;
			to->r_ac = coor_xyz_distance(&a, &c);
			to->r_ab = coor_xyz_distance(&a, &b);
		break;

		case 'b':
			c.x = 0.0;
			c.y = from->r/2.0;
			c.z = 0.0;

			a.x =  0.0;
			a.y = -c.y;
			a.z =  0.0;

			b.x = 0.0;

			b.y = (c.y*mass(atom_c) + a.y*mass(atom_a))/(mass(atom_c) + mass(atom_a))
			    + from->R*sin(from->theta*M_PI/180.0);

			b.z = from->R*cos(from->theta*M_PI/180.0);

			to->r_ac = from->r;
			to->r_bc = coor_xyz_distance(&b, &c);
			to->r_ab = coor_xyz_distance(&a, &b);
		break;

		case 'c':
			a.x = 0.0;
			a.y = from->r/2.0;
			a.z = 0.0;

			b.x =  0.0;
			b.y = -a.y;
			b.z =  0.0;

			c.x = 0.0;

			c.y = (a.y*mass(atom_a) + b.y*mass(atom_b))/(mass(atom_a) + mass(atom_b))
			    + from->R*sin(from->theta*M_PI/180.0);

			c.z = from->R*cos(from->theta*M_PI/180.0);

			to->r_ab = from->r;
			to->r_bc = coor_xyz_distance(&b, &c);
			to->r_ac = coor_xyz_distance(&a, &c);
		break;

		default:
			PRINT_ERROR("invalid arrangement %c\n", from->arrang)
			exit(EXIT_FAILURE);
	}
}
