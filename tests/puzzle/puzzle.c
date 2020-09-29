/* $Id: puzzle.c,v 1.2 1996/01/16 14:17:04 chris Exp $ */
#define size 511
#define classmax 3
#define typemax 12
#define d 8
#define true 1
#define false 0

#include "bench.h"

/*************************************************************
 *  puzzle.c
 *	The standard puzzle benchmark
 */

char   piececount[classmax + 1];
char   class[typemax+1];
short  piecemax[typemax+1];
char   puzzle[size+1];
char   p[typemax+1][size+1];

short	m,n;
int	kount;

int fit(i,j)
	int i,j;
{
	short k;

	for (k = 0; k <= piecemax[i]; k++)
		if (p[i][k]) if (puzzle[j+k]) return (false);
	return(true);
}

int place(i,j)
	int i,j;
{
	short k;
	for (k = 0; k <= piecemax[i];  k++)
		if (p[i][k]) puzzle[j+k] = true;
	piececount[class[i]] = piececount[class[i]] - 1;
	for (k = j; k <= size; k++) 
		if ( ! puzzle[k] ) return(k);
	/*printf("puzzle filled\n");*/;
	return(0);
}

void remove_(int i, int j)
{
	short k;

	for (k = 0; k <= piecemax[i]; k++)
		if (p[i][k]) puzzle[j+k] = false;
	piececount[class[i]] = piececount[class[i]] + 1;
}

int trial(j)
	int j;
{
	int i,k;
	char trialval;

	for (i = 0; i <= typemax; i++) {
		if (piececount[class[i]])
			if (fit(i,j)) {
				k = place(i,j);
				if (trial(k) || (k == 0)) {
					/*printf("piece %d at %d\n", i + 1, k + 1);*/;
					kount = kount + 1;
					return(true);
				} else
					remove_(i, j);
			}
	}
	kount = kount + 1;
	return(false);
}

void puzzle_(void) 
{
	int i,j,k;

	for (m = 0; m <= size; m++)
		puzzle[m] = 1;
	for (i = 1; i <= 5; i++)
		for (j = 1; j <= 5; j++)
			for (k = 1; k <= 5; k++)
				puzzle[i+8*(j+8*k)] = 0;
	for (i = 0; i <= 12; i++)
		for (m = 0; m <= 511; m++)
			p[i][m] = 0;
	for (i = 0; i <= 3; i++)
		for (j = 0; j <= 1; j++)
			for (k = 0; k <= 0; k++)
				p[0][i+8*(j+8*k)] = 1;
	class[0] = 0;
	piecemax[0] = 3 + 8 * 1 + 8 * 8 * 0;
	for (i = 0; i <= 1; i++)
		for (j = 0; j <= 0; j++)
			for (k = 0; k <= 3; k++)
				p[1][i+8*(j+8*k)] = 1;
	class[1] = 0;
	piecemax[1] = 1 + 8 * 0 + 8 * 8 * 3;
	for (i = 0; i <= 0; i++)
		for (j = 0; j <= 3; j++)
			for (k = 0; k <= 1; k++)
				p[2][i+8*(j+8*k)] = 1;
	class[2] = 0;
	piecemax[2] = 0 + 8 * 3 + 8 * 8 * 1;
	for (i = 0; i <= 1; i++)
		for (j = 0; j <= 3; j++)
			for (k = 0; k <= 0; k++)
				p[3][i+8*(j+8*k)] = 1;
	class[3] = 0;
	piecemax[3] = 1 + 8 * 3 + 8 * 8 * 0;
	for (i = 0; i <= 3; i++)
		for (j = 0; j <= 0; j++)
			for (k = 0; k <= 1; k++)
				p[4][i+8*(j+8*k)] = 1;
	class[4] = 0;
	piecemax[4] = 3 + 8 * 0 + 8 * 8 * 1;
	for (i = 0; i <= 0; i++)
		for (j = 0; j <= 1; j++)
			for (k = 0; k <= 3; k++)
				p[5][i+8*(j+8*k)] = 1;
	class[5] = 0;
	piecemax[5] = 0 + 8 * 1 + 8 * 8 * 3;
	for (i = 0; i <= 2; i++)
		for (j = 0; j <= 0; j++)
			for (k = 0; k <= 0; k++)
				p[6][i+8*(j+8*k)] = 1;
	class[6] = 1;
	piecemax[6] = 2 + 8 * 0 + 8 * 8 * 0;
	for (i = 0; i <= 0; i++)
		for (j = 0; j <= 2; j++)
			for (k = 0; k <= 0; k++)
				p[7][i+8*(j+8*k)] = 1;
	class[7] = 1;
	piecemax[7] = 0 + 8 * 2 + 8 * 8 * 0;
	for (i = 0; i <= 0; i++)
		for (j = 0; j <= 0; j++)
			for (k = 0; k <= 2; k++)
				p[8][i+8*(j+8*k)] = 1;
	class[8] = 1;
	piecemax[8] = 0 + 8 * 0 + 8 * 8 * 2;
	for (i = 0; i <= 1; i++)
		for (j = 0; j <= 1; j++)
			for (k = 0; k <= 0; k++)
				p[9][i+8*(j+8*k)] = 1;
	class[9] = 2;
	piecemax[9] = 1 + 8 * 1 + 8 * 8 * 0;
	for (i = 0; i <= 1; i++)
		for (j = 0; j <= 0; j++)
			for (k = 0; k <= 1; k++)
				p[10][i+8*(j+8*k)] = 1;
	class[10] = 2;
	piecemax[10] = 1 + 8 * 0 + 8 * 8 * 1;
	for (i = 0; i <= 0; i++)
		for (j = 0; j <= 1; j++)
			for (k = 0; k <= 1; k++)
				p[11][i+8*(j+8*k)] = 1;
	class[11] = 2;
	piecemax[11] = 0 + 8 * 1 + 8 * 8 * 1;
	for (i = 0; i <= 1; i++)
		for (j = 0; j <= 1; j++)
			for (k = 0; k <= 1; k++)
				p[12][i+8*(j+8*k)] = 1;
	class[12] = 3; 
	piecemax[12] = 1 + 8 * 1 + 8 * 8 * 1;
	piececount[0] = 13;
	piececount[1] = 3;
	piececount[2] = 1;
	piececount[3] = 1;
	m = 1 + 8 * (1 + 8 * 1);
	kount = 0;
	if (fit(0,m))
		n = place(0,m);
	else {
		printf("error 1\n");
	}
	if (trial(n)) {
		printf("success in %d trials\n", kount);
	} else {
		printf("failure\n");
	}
}

SINGLE_BENCHMARK(puzzle_())
