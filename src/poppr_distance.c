/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
# This software was authored by Zhian N. Kamvar and Javier F. Tabima, graduate 
# students at Oregon State University; and Dr. Nik Grünwald, an employee of 
# USDA-ARS.
#
# Permission to use, copy, modify, and distribute this software and its
# documentation for educational, research and non-profit purposes, without fee, 
# and without a written agreement is hereby granted, provided that the statement
# above is incorporated into the material, giving appropriate attribution to the
# authors.
#
# Permission to incorporate this software into commercial products may be
# obtained by contacting USDA ARS and OREGON STATE UNIVERSITY Office for 
# Commercialization and Corporate Development.
#
# The software program and documentation are supplied "as is", without any
# accompanying services from the USDA or the University. USDA ARS or the 
# University do not warrant that the operation of the program will be 
# uninterrupted or error-free. The end-user understands that the program was 
# developed for research purposes and is advised not to rely exclusively on the 
# program for any reason.
#
# IN NO EVENT SHALL USDA ARS OR OREGON STATE UNIVERSITY BE LIABLE TO ANY PARTY 
# FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES, INCLUDING
# LOST PROFITS, ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, 
# EVEN IF THE OREGON STATE UNIVERSITY HAS BEEN ADVISED OF THE POSSIBILITY OF 
# SUCH DAMAGE. USDA ARS OR OREGON STATE UNIVERSITY SPECIFICALLY DISCLAIMS ANY 
# WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
# MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE AND ANY STATUTORY 
# WARRANTY OF NON-INFRINGEMENT. THE SOFTWARE PROVIDED HEREUNDER IS ON AN "AS IS"
# BASIS, AND USDA ARS AND OREGON STATE UNIVERSITY HAVE NO OBLIGATIONS TO PROVIDE
# MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS. 
#
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
#include <stdio.h>
#include <Rinternals.h>
#include <Rdefines.h>
#include <R.h>
#include <math.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
int perm_count;
double bruvo_dist(int *in, int *nall, int *perm, int *woo);
double test_bruvo_dist(int *in, int *nall, int *perm, int *woo, int *loss, int *add);
void permute(int *a, int i, int n, int *c);
int fact(int x);
void pass_vector(int *pointy, int *pointynumber);
double mindist(int perms, int alleles, int *perm, double *dist);
/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Calculates the root product of pairwise comparisons of each of the variances of
each locus.

Input: A vector of length n where n is the number of loci in the population.
Output: A vector of length n*(n-1)/2
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
SEXP pairwise_covar(SEXP pair_vec)
{
	int I, i, j, count;
	SEXP Rout;
	I = length(pair_vec);
	pair_vec = coerceVector(pair_vec, REALSXP);
	PROTECT(Rout = allocVector(REALSXP, (I*(I-1)/2) ));
	count = 0;
	for(i = 0; i < I-1; i++)
	{
		for(j = i+1; j < I; j++)
		{
			REAL(Rout)[count++] = sqrt(REAL(pair_vec)[i] * REAL(pair_vec)[j]);
		}
	}
	UNPROTECT(1);
	return Rout;
}
/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Calculates the absolute yes or no distance of the alleles at a locus. Note
that since this is based on integers (due to the absolute value function
conversion), The matrix incoming should only be integers, so if it's a diploid
organism, multiply the matrix by 2. Divide the result by 2 to get the distances.

Input: An n x m matrix where n is the number of individuals, and m is the number
of alleles at a single locus. 

Output: A vector of length n*(n-1)/2
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
SEXP pairdiffs(SEXP freq_mat)
{
	int I, J, i, j, z, count, P;
	SEXP Rout;
	SEXP Rdim;
	SEXP pair_matrix;
	Rdim = getAttrib(freq_mat, R_DimSymbol);
	I = INTEGER(Rdim)[0]; // Rows
	J = INTEGER(Rdim)[1]; // Columns
	PROTECT(pair_matrix = allocVector(REALSXP, J*2));
	count = 0;
	PROTECT(Rout = allocVector(INTSXP, I*(I-1)/2));
	for(i = 0; i < I-1; i++)
	{
		for(z = 0; z < J; z++)
		{
			REAL(pair_matrix)[z] = REAL(freq_mat)[i+(I)*z];
		}
		for(j = i+1; j < I; j++)
		{
			P = 0;
			for(z = 0; z < J; z++)
			{
				if(ISNA(REAL(pair_matrix)[0]) || ISNA(REAL(freq_mat)[j+(I)*z]))
				{
					P = 0;
					break;
				}
				P += abs(REAL(pair_matrix)[z] - REAL(freq_mat)[j+(I)*z]);
			}
			INTEGER(Rout)[count++] = P;
		}
	}
	UNPROTECT(2);
	return Rout;
}
/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
permuto will return a vector of all permutations needed for bruvo's distance.
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
SEXP permuto(SEXP perm)
{
	int permutations, i, per;
	SEXP Rval;
	/* 
		IMPORTANT: INITIALIZE THE COUNTER. THE POINTER IS NOT RELEASED FROM
		MEMORY OTHERWISE.
	*/
	perm_count = 0;
	perm = coerceVector(perm, INTSXP);
	per = INTEGER(perm)[0];
	int allele_array[per];
	permutations = fact(per)*per;
	for(i = 0; i < per; i++)
	{
		allele_array[i] = i;
	}
	PROTECT(Rval = allocVector(INTSXP, permutations));
	permute(allele_array, 0, per-1, INTEGER(Rval));
	UNPROTECT(1);
	return Rval;
}
/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Calculates Bruvo's distance over a matrix of individuals by loci. This is
calcluated regardless of ploidy. For more information, see Bruvo et al. 2006

Parameters:
bruvo_mat - a matrix of individuals by loci, one column per allele.
permutations - a vector of indeces for permuting the number of alleles. 
alleles - the ploidy of the population. 

Returns:

A matrix of n*(n-1)/2 rows and the same number of columns as bruvo_mat

Notes:
Currently bruvo_mat should not contain NAs. The way to deal with them, as they
are based off of microsatellites, is to replace them with zero. The main
distance algorithm in turn will return a distance of 100 for any individuals
with missing data. In the wrapping R function, 100s will be converted to NAs
and then the average over all loci will be taken. 
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

SEXP single_bruvo(SEXP b_mat, SEXP permutations, SEXP alleles, SEXP loss, SEXP add)
{
	int A, P, *pA, *pP;
	SEXP Rval;
	//SEXP Rdim;
	P = length(permutations);
	alleles = coerceVector(alleles, INTSXP);
	loss = coerceVector(loss, INTSXP);
	add = coerceVector(add, INTSXP);
	A = INTEGER(alleles)[0];
	pA = &A;
	pP = &P;
	b_mat = coerceVector(b_mat, INTSXP);
	permutations = coerceVector(permutations, INTSXP);
	PROTECT(Rval = allocVector(REALSXP, 1));
	REAL(Rval)[0] = test_bruvo_dist(INTEGER(b_mat), pA, INTEGER(permutations),
                                    pP, INTEGER(loss), INTEGER(add));
	UNPROTECT(1);
	return Rval;
    
}
SEXP bruvo_distance(SEXP bruvo_mat, SEXP permutations, SEXP alleles)
{
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	I = number of rows in bruvo_mat
	J = number of columns in bruvo_mat
	A = ploidy
	P = A*A!
	*pA = pointer to A
	*pP = pointer to P
	
	A matrix in R is built row by row. That's why there is a triple 'for' loop.
	~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
	int I, J, A, P, i, j, a, count = 0, *pA, *pP;//, add, loss, *padd, *ploss;
	//Initialization of R vectors.
	SEXP Rdim;
	SEXP Rval;
	SEXP pair_matrix;
	P = length(permutations);
	Rdim = getAttrib(bruvo_mat, R_DimSymbol);
	I = INTEGER(Rdim)[0]; // Rows
	J = INTEGER(Rdim)[1]; // Columns
	alleles = coerceVector(alleles, INTSXP);
	A = INTEGER(alleles)[0];
	pA = &A;
	pP = &P;
	//add = 1;
	//loss = 1;
	//padd = &add;
	//ploss = &loss;
	bruvo_mat = coerceVector(bruvo_mat, INTSXP);
	permutations = coerceVector(permutations, INTSXP);
	// Protecting the vectors that will be modified. Rval is the output
	PROTECT(Rval = allocMatrix(REALSXP, I*(I-1)/2, J/A ));
	// pair_matrix is the input for individual bruvo distances. 
	PROTECT(pair_matrix = allocVector(INTSXP, 2*A));
	
	/*	First step, loop over each set of columns defined by the number of
		alleles. So for a diploid, it will loop over the first two columns. */
	for(a = 0; a < J; a += A)
	{
	//	Looping over n-1 individuals. 
		for(i = 0; i < I; i++)
		{
			int z;
			//	Populating the allele array from the reference individual.
			for(z = 0; z < A; z++) 
			{
			/*	i = reference individual, a = the locus, z = the allele
				(a+z)*I = The combination of locus, allele and total number
				of individuals to move across the matrix. 
				This will supply the reference genotype. */
				INTEGER(pair_matrix)[z] = INTEGER(bruvo_mat)[i+(a+z)*I];
			}
			//	Looping over individuals for pairwise comparison.
			for(j = i+1; j < I; j++)
			{
				//	Populating allele array from the comparison individual.
				for(z = A; z < A*2 ; z++)
				{
					INTEGER(pair_matrix)[z] = INTEGER(bruvo_mat)[j+(a+z-A)*I];
				}
				// Calculating Bruvo's distance over these two. 
				REAL(Rval)[count++] = bruvo_dist(INTEGER(pair_matrix), pA,
                                                 INTEGER(permutations), pP);//, padd, ploss);
			}
		}
	}
	UNPROTECT(2);
	return Rval;
}

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	Internal C Functions
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

/* 
	The algorithm for the permutation function is modified from:
	http://www.geeksforgeeks.org/archives/767 */

/* Function to swap values at two pointers */
void swap (int *x, int *y)
{
	int temp;
	temp = *x;
	*x = *y;
	*y = temp;
}

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ 
	Function to print permutations of string
		This function takes four parameters:
		1. String
		2. Starting index of the string
		3. Ending index of the string. 
		4. pointer to array of size n*n! 
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
void permute(int *a, int i, int n, int *c) 
{
	int j;
	if (i == n)
	{
		/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
			'a' will be the array containing the numeric sequence to be
			permuted. It will be reshuffled into a new pattern each
			time it reaches this control structure. To place the value
			into the array 'c', the pointer for a needs to be incremented
			over all its elements.
		~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
		perm_count += n+1;
		int ind = perm_count;
		for(j=0; j<=n; j++)
		{
			c[--ind] = *(a + j);
		}
	}
	else
	{
		for (j = i; j <= n; j++)
		{
			swap((a + i), (a + j));
			permute(a, i + 1, n, c);
			swap((a + i), (a + j)); //backtrack
		}
	}
}

/* A factorial function for calculating permutations */
int fact(int x)
{
	int f = 1;
	int u;
	for (u = x; u > 1; u--)
	{
		f *= u;
	}
	return f;
}

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

	This will calculate bruvo's distance between two individuals. 
	All that needs to be done from here is to have it do the pairwise
	calculations. 

	NOTE: The input needs to be divided by the repeat length beforehand for this
	to work. 

	in: a matrix of two individuals
	out: a double value that will be the output from bruvo's distance.
	n: number of individuals(2)
	nall / p: number of alleles
	perm: a vector from the permn function in R
	woo: p * p!
	minn: is a rolling counter of the minimum between allele compairsons.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
double bruvo_dist(int *in, int *nall, int *perm, int *woo)
{
	int i, j, counter=0, n = 2, p = *nall, w = *woo, genos[2][p];
	double dist[p][p], da, res = 0, minn=100;
	// reconstruct the genotype table.
	for(i=0; i < n; i++)
	{
		for(j = 0; j < p; j++)
		{
			// Missing data will return with distance of 100
			if(in[counter] == 0)
			{
			/*	THIS WILL BE THE PLACE TO PUT A NEW FUNCTION FOR SPECIAL
				CASES OF BRUVO'S DISTANCE */
				return minn;
			}
			else
			{
				genos[i][j] = in[counter++];
			}
		}
	}

	// Construct distance matrix of 1 - 2^{-|x|}
	for(j = 0; j < p; j++)
	{
		for(i=0; i < p; i++)
		{
			da = 1- pow(2 , -abs(genos[0][i] - genos[1][j]));
			dist[i][j] = da;
		}
	}

	//	Calculate the smallest s, which is the minimum distance among alleles.
	for(i = 0; i < w; i += p)
	{
		for(j = 0; j < p; j++)
		{
			if (j == 0)
			{
				res = dist[*perm++][j];
			}
			else
			{
				res += dist[*perm++][j];
			}
		}
		/*	Checking if the new calculated distance is smaller than the smallest
			distance seen. */
		if ( res < minn )
		{
			minn = res;
		}
	}
	return minn/p;
}




double test_bruvo_dist(int *in, int *nall, int *perm, int *woo, int *loss, int *add)
{
	int i, j, counter = 0, n = 2, p = *nall, w = *woo, loss_indicator = *loss, 
		add_indicator = *add, genos[2][p], zerocatch[2], zero_ind[2][p],
		zerodiff, full_ind[2][p], full_count = 0;
	double dist[p][p], da, minn = 100, *distp;
	// reconstruct the genotype table.
	zerocatch[0] = 0;
	zerocatch[1] = 0;
	for(i=0; i < n; i++)
	{
		for(j = 0; j < p; j++)
		{
			// Catch missing data here.
			if(in[counter] == 0)
			{
				if (zerocatch[i] == p - 1)
				{
					return minn;
				}
				zerocatch[i] += 1;
				zero_ind[i][zerocatch[i] - 1] = j;
			}
			else
			{
				full_ind[i][full_count++] = j;
			}
			genos[i][j] = in[counter++];
			
		}
	}
	zerodiff = abs(zerocatch[0] - zerocatch[1]);
	/*==========================================================================
	* Removing superfluous zeroes from the data. This is in the case that both
	* of the genotypes contain one or more zeroes.
	==========================================================================*/
	if (zerocatch[0] > 0 && zerocatch[1] > 0)
	{
		int larger = 0, smaller = 1, reduction = 0, i, j, 
			zero_counter, *perm_array, *new_genop;

		if (zerodiff == 0)
		{
			reduction = p - zerocatch[0];
		}
		else
		{
			if(zerocatch[0] < zerocatch[1])
			{
				larger = 1;
				smaller = 0;
			}
			reduction = p - (zerocatch[larger] - zerodiff);
		}
		int new_alleles[reduction];
		for (i = 0; i < reduction; i++)
		{
			new_alleles[i] = i;
		}
		w = fact(reduction) * reduction;
		perm_array = (int *) malloc(w * sizeof(int));
		perm_count = 0;
		permute(new_alleles, 0, reduction - 1, perm_array);
		// rebuild the array and make a pointer.
		int new_geno[reduction*n];
		counter = 0;
		for(i=0; i < n; i++)
		{
			zero_counter = zerocatch[smaller];
			for(j = 0; j < p; j++)
			{
				if (genos[i][j] == 0 && zero_counter > 0)
				{
					zero_counter--;
				}
				else
				{
					new_geno[counter++] = genos[i][j];
				}
			}
		}
		new_genop = (int *) &new_geno;
		
		minn = test_bruvo_dist(new_genop, &reduction, perm_array, &w, 
										&loss_indicator, &add_indicator);
		free(perm_array);
		return minn;
	}

	// Construct distance matrix of 1 - 2^{-|x|}.
	// This is constructed column by column. Genotype 1 in the rows. Genotype 2
	// in the columns.
	for(j = 0; j < p; j++)
	{
		for(i = 0; i < p; i++)
		{
			da = 1 - pow(2, -abs(genos[0][i] - genos[1][j]));
			dist[i][j] = da;
		}
	}
	// This avoids warning: assignment from incompatible pointer type
	distp = (double *) &dist;
	/*	Test code comparing current status to polysat's Bruvo2.distance:
	****
================================================================================
.Call("single_bruvo", c(20,23,24,0,20,24,26,43), .Call("permuto", 4), 4, 0, 0)
Bruvo2.distance(c(20,23,24), c(20,24,26,43), usatnt=1, loss=FALSE, add=FALSE)
.Call("single_bruvo", c(20,23,24,0,20,24,26,43), .Call("permuto", 4), 4, 1, 0)
Bruvo2.distance(c(20,23,24), c(20,24,26,43), usatnt=1, loss=T, add=FALSE)
.Call("single_bruvo", c(20,23,24,0,20,24,26,43), .Call("permuto", 4), 4, 0, 1)
Bruvo2.distance(c(20,23,24), c(20,24,26,43), usatnt=1, loss=F, add=T)
.Call("single_bruvo", c(20,23,24,0,20,24,26,43), .Call("permuto", 4), 4, 1, 1)
Bruvo2.distance(c(20,23,24), c(20,24,26,43), usatnt=1, loss=T, add=T)
================================================================================
	****
	*/
	if(zerocatch[0] > 0 || zerocatch[1] > 0)
	{
		int *genop, ind, miss_ind = 1, full_ind = 0, z;
		double genome_add_sum = 0, genome_loss_sum = 0;//, derp = 0;
		genop = (int *) &genos;
		if (zerocatch[0] > 0) // The rows contain the zero value
		{
			miss_ind = 0;
			full_ind = 1; 
		}
		ind = zero_ind[miss_ind][0];
		/*======================================================================
		*	INFINITE MODEL
		*	Infinite model will simply replace the distance of the comparisons
		*	containing the missing allele to 1.
		======================================================================*/
		if(loss_indicator != 1 && add_indicator != 1)
		{
			for (z = 0; z < zerocatch[miss_ind]; z++)
			{
				ind = zero_ind[miss_ind][z];
				if (zerocatch[0] > 0)
				{
					for (j = 0; j < p; j++)
					{
						dist[ind][j] = 1;
					}
				}
				else
				{
					for (j = 0; j < p; j++)
					{
						dist[j][ind] = 1;
					}						
				}
			}
			return mindist(w, p, perm, distp)/p;
		}
		/*======================================================================
		*	GENOME ADDITION MODEL
		*	Genome Addition model uses the observed values of the short 
		*	genotype for the replacement allele. This is achieved by simply
		*	shifting the columns or rows of the distance matrix and 
		*	recalculating the minimum distance. 
		======================================================================*/
		if (add_indicator == 1)
		{
			for (i = 0; i < p; i++)
			{
				if (i == ind)
				{
					goto next1;
				}
				if (zerocatch[0] > 0)
				{
					for (j = 0; j < p; j++)
					{
						dist[ind][j] = dist[i][j];
					}
				}
				else
				{
					for (j = 0; j < p; j++)
					{
						dist[j][ind] = dist[j][i];
					}						
				}
				/*
				derp = mindist(w, p, perm, distp)*p;
				genome_add_sum += derp;
				printf("Genome Addition Distance: %11f\n", derp);
				*/
				genome_add_sum += mindist(w, p, perm, distp);
				next1:;	
			}
		}
		/*======================================================================
		*	GENOME LOSS MODEL
		*	Genome Loss model uses the alleles from the larger genotype to
		*	reconstruct the allelic state of the smaller. This means that
		*	they need to be replaced and passed through the function again.
		======================================================================*/
		if (loss_indicator == 1)
		{
			for (i = 0; i < p; i++)
			{
				//printf("Origininal: %d\tReplacement: %d\n", genos[miss_ind][ind], genos[full_ind][i]);
				genos[miss_ind][ind] = genos[full_ind][i];
				/*
				derp = test_bruvo_dist(genop, &p, perm, &w)*p;
				genome_loss_sum += derp;
				printf("Genome Loss Distance: %11f\n", derp);
				*/
				//	Note: These need to be multiplied by p since the result has been
				//	divided by p 
				genome_loss_sum += test_bruvo_dist(genop, &p, perm, &w, 
										&loss_indicator, &add_indicator)*p;
			}
		}
		genome_loss_sum = genome_loss_sum/p;
		genome_add_sum = genome_add_sum/(p-1);
		int comparison_factor = loss_indicator + add_indicator;
		//printf("Genome Loss Model: %11f\tGenome Addition Model: %11f\n", 
		//genome_loss_sum/p, genome_add_sum/p);
		return (genome_add_sum + genome_loss_sum)/(p*comparison_factor);
	}
	return mindist(w, p, perm, distp)/p;
}


void pass_vector(int *pointy, int *pointynumber)
{
	int i, pn = *pointynumber;
	printf("pointers: %d, %d\n", *(pointy), pn);

	for (i = 0; i < pn; i++)
	{
		printf("I'm in another function!\t%d\n", *(pointy + i));
	}
}
/*==============================================================================
* A function for dealing with the genome addition model. This will replace the
* portions of the distance matrix generated via the missing values with columns
* (or rows) of observed values in a combinatorial way. This will involve 
* recursion.
* For safe keeping:
* dist[zero_ind[z] + alleles*j] = dist[rep_int + alleles*j];
* dist[j + alleles*zero_ind[z]] = dist[j + alleles*rep_int];
* 
* Arguments:
*	perms - number of permutations (passed to mindist)
*	alleles - number of maximum alleles (also passed to mindist)
*	*perm - permutation array (passed to mindist)
*	*dist - distance matrix to be manipulated (also passed to mindist)
*	
*	zeroes - number of zeroes present in the shorter genotype.
*	*zero_ind - array containing indices of the zero values of the short geno
*	curr_zero - the index of the current zero index for zero_ind
*	miss_ind - the index for the genotype with missing data. Necessary for
*				determining rows or columns
*	*replacement - array containing indices of replacement genotypes.
*	inds - number of replacement genotypes.
*	curr_ind - the index of the current replacement genotype.
*
*	*genome_add_sum - pointer to the total number of the genome addition model. 

void genome_add_calc(int perms, int alleles, int *perm, double *dist, 
	int zeroes, int *zero_ind, int curr_zero, int miss_ind, int *replacement, 
	int inds, int curr_ind, double *genome_add_sum)
{
	int i,j;

	//==========================================================================
	// Part 1: fill one row/column of the matrix.
	//==========================================================================
	if(miss_ind == 0)
	{
		for (j = 0; j < alleles; j++)
		{
			dist[zero_ind[curr_zero] + alleles*j] = 
				dist[replacement[curr_ind] + alleles*j];
		}
	}
	else
	{
		for (j = 0; j < alleles; j++)
		{
			dist[j + alleles*zero_ind[curr_zero]] = 
				dist[j + alleles*replacement[curr_ind]];		
		}
	}
	//==========================================================================
	// Part 2: Iterate through the rest of the possible combinations.
	//
	// The first for loop iterates through all possible individuals.
	// The first if loop will check if there are any more slots to be filled.
	// if there aren't, then the minimum distance will be calculated on the
	// matrix as it stands and then the sum will be returned. 
	//==========================================================================
	for (i = curr_ind; curr_ind < inds; curr_ind++)
	{
		if (curr_zero < zeroes-1)
		{
			// Note: curr_zero is incremented here. 
			genome_add_calc(perms, alleles, perm, dist, zeroes, zero_ind, 
				curr_zero++, miss_ind, replacement, inds, curr_ind, 
				genome_add_sum);
		}
		else
		{
			*genome_add_sum += mindist(perms, alleles, perm, dist);
			return;
		}
	}
}
==============================================================================*/

double mindist(int perms, int alleles, int *perm, double *dist)
{
	int i, j, w = perms, p = alleles, counter = 0;
	double res = 0, minn = 100;
	for(i = 0; i < w; i += p)
	{
		for(j = 0; j < p; j++)
		{
			if (j == 0)
			{
				res = dist[*(perm + counter++) + p*j];
			}
			else
			{
				res += dist[*(perm + counter++) + p*j];
			}
		}
		/*	Checking if the new calculated distance is smaller than the smallest
		 distance seen. */
		if ( res < minn )
		{
			minn = res;
		}
	}
	return minn;
}
