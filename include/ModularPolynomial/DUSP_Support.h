/**
 * Supporting methods for dense univariate polynomials over small-prime fields (DUSP)
 *  written in pure C.
 * 
 * The data structure of a DUSP polynomial is an alternating array of coefficients
 * in an increasing order. For example: 4x^5 + 2x^3 + 7 is stored like 7 + 0 x^1 + 0 x^2 
 * + 2 x^3 + 0 x^4 + 4 x^5, as an alternating array of (7, 0, 0, 2, 0, 4) and lt = 5. 
 *
 * We use pure C functions of ./FiniteFields/SmallPrimeField_Support.h for basic  
 * operation over a small prime field (bitlength = 64) 
 * coefficients of a polynomial are in Montgomery form where R is 2^64; 
 * polynomial, prime and (-prime^{-1} mod R) are shown a, prime and prime_inv,
 * which is defined in a structure named Prime_prt.
 *
 * Name of functions with InForm at the end, refer to the functions that their inputs and 
 * outputs are in montgomery form. 
 *
 * Name of functions with "_inp" at the end, refer to the functions implemented in-place.
 *
 */


#ifndef _DUSP_SUPPORT_H_
#define _DUSP_SUPPORT_H_

#ifdef __cplusplus
extern "C" {
#endif
	
#include <stdio.h>
#include <stdlib.h>
// #include <string.h>

#include "../FiniteFields/SmallPrimeField_Support.h"
#include "../FiniteFields/GFPF_Support.h"
// #include "../FiniteFields/small_prime_field_fft.h"
// #include "../FiniteFields/fourier_primes_u64.h"
// #include "../FiniteFields/gfpf_arithmetic.h"
/*********************************************
 ** Polynomial Definition and Helper Functions  
 *************************/

typedef long int  polysize_t; // polynomial size
// typedef unsigned long long int usfixn64; // element type w.r.t ./FiniteField/gfpf_arithmetic.h
typedef long long int elem_t; // element type w.r.t ./FiniteField/SmallPrimeField_Support.h
typedef long long int prime_t; // prime type w.r.t ./FiniteField/SmallPrimeField_Support.h

// #define R (1LL << 64)
#define PRIME_BIT_LENGTH (32)
#define KARATSUBA_CROSSOVER (5) //    
#define HALFGCD_CROSSOVER (32)   // In ntl : 25  (NTL_ZZ_pX_HalfGCD_CROSSOVER)
#define PLAINGCD_CROSSOVER (128) // In ntl : 180 (NTL_ZZ_pX_GCD_CROSSOVER)
#define PLAINMUL_CROSSOVER (128) // is min(deg(a), deg(b)) otherwise need fft .
#define PLAINDIV_CROSSOVER (512) 
#define PLAINRES_CROSSOVER (128) // by experience w.r.t. benchmarks
#define FFTMUL_CROSSOVER (512) 

#define FURER_FFT (1)

#define DEBUG_PRINT_LINES (0) 
#define COUNT_SUBPROGRAMS (0)
#define TIMER_SUBPROGRAMS (0)

typedef struct duspoly {
    elem_t* elems;       // an alternating array of coefficients
    polysize_t alloc;    // size of allocation
    polysize_t lt;       // leading term
} duspoly_t;

#define POLY_ALLOC(A) ((A)->alloc)
#define POLY_LT(A) ((A)->lt)

#define MAX_spX(x ,y) (((x) > (y)) ? (x) : (y))
#define MIN_spX(x ,y) (((x) < (y)) ? (x) : (y))

static inline duspoly_t* makePolynomial_spX (polysize_t alloc) {

    duspoly_t* poly = (duspoly_t*) malloc (sizeof(duspoly_t));
    poly->elems = (elem_t*) calloc (alloc, sizeof(elem_t));
    poly->alloc = alloc;
    poly->lt = 0;
    // poly->elems[0] = 0;
    
    return poly;
}

/**
 * Free polynomial, a.  
 *
 */ 

static inline void freePolynomial_spX (duspoly_t** a)
{
    if (*a == NULL) {
        return;
    }

    if ((*a)->alloc == 0) {    
        free (*a); 
        return;
    }

    free((*a)->elems); 
    free(*a);

    *a = NULL;
}


/**
 * Deep Copy of polynomial, a.
 */
static inline duspoly_t* deepCopyPolynomial_spX (const duspoly_t* a)
{
    if (a == NULL || POLY_ALLOC(a) == 0) {
	   return NULL;
    }
    
    duspoly_t* dcp = makePolynomial_spX (POLY_ALLOC (a));    
    dcp->lt = POLY_LT(a);

    memmove(dcp->elems, a->elems, POLY_ALLOC(a)*sizeof(elem_t));
        
    return dcp;
}

/**
 * Construct a zero polynomial
 * output is 0*X^0.
 */
static inline duspoly_t* zeroPolynomial_spX ()
{

    duspoly_t* poly = makePolynomial_spX (1);
    
    return poly;
}


/**
 * Construct a constant polynomial
 * output is (c mod pr)*X^0.
 */
static inline duspoly_t* constPolynomial_spX (elem_t c, const Prime_ptr* Pptr)
{
    duspoly_t* poly = makePolynomial_spX (1);
    poly->elems[0] = smallprimefield_convert_in (c, Pptr);  // smallprimefield_convert_in (c, Pptr);
    
    return poly;
}

/*Supposed that c is in Montgomery form*/
static inline duspoly_t* constPolynomialInForm_spX (elem_t c, const Prime_ptr* Pptr)
{
    duspoly_t* poly = makePolynomial_spX (1);
    poly->elems[0] = c % Pptr->prime; // BigPrime->TODO

    return poly;
}

	
/**
 * Construct a polynomial of degree 1.
 * output is 0*X^0 + convert_in_mongomery(1)*X^1. 
 */
static inline duspoly_t* Xpolynomial_spX (const Prime_ptr* Pptr)
{
    duspoly_t* poly = makePolynomial_spX (2);
    poly->lt = 1;
    poly->elems[0] = 0;
    poly->elems[1] = smallprimefield_convert_in (1, Pptr);
    
    return poly;
}

/*Supposed that the coefficient of X^1 is 1 in Montgomery form.*/
static inline duspoly_t* XpolynomialInForm_spX ()
{
    duspoly_t* poly = makePolynomial_spX (2);
    poly->lt = 1;
    poly->elems[0] = 0;
    poly->elems[1] = 1;
    
    return poly;
}

/**
 * Construct polynomial x^n. 
 */
static inline duspoly_t* makePowerOfXPolynomialInForm_spX (polysize_t n, const Prime_ptr* Pptr) 
{
    if (n < 1) {
        return constPolynomial_spX (1, Pptr);
    }

    if (n == 1) {
        return Xpolynomial_spX (Pptr);
    }

    duspoly_t* xn = makePolynomial_spX (n+1);
    xn->elems[n] = 1;
    xn->lt = n;

    return xn;
}


/**
 * isZero function 
 * if a = NULL, or poly = 0*X^0 : this funciton returns -1 and 1, 
 * otherwise returns 0.
 */
static inline int isZero_spX (const duspoly_t* a)
{
    if (a == NULL || POLY_ALLOC(a) == 0 || a->elems == NULL){
        return -1;
    } else if (POLY_LT(a) == 0 && a->elems[0] == 0) {
        return 1;
    }
    return 0;
}


/**
 * isOne function 
 * if a = 1*X^1 : this function returns 1, otherwise returns 0.
 */
static inline int isOne_spX (const duspoly_t* a)
{
    if (a == NULL || POLY_ALLOC(a) == 0) {
        return 0;
    }
    
    if (POLY_LT(a) == 0 && a->elems[0] == 1) {
        return 1;
    }
    return 0;
}

/*Supposed that a is in Montgomery form and looking for convert_in_montgomery(1)*/
static inline int isOneInForm_spX (const duspoly_t* a, const Prime_ptr* Pptr)
{
    if (a == NULL || POLY_ALLOC(a) == 0) {
        return 0;
    }
        
    if (POLY_LT(a) == 0 && a->elems[0] == smallprimefield_convert_in (1, Pptr)) {
        return 1;
    }
    return 0;
}

/**
 * isConstant function 
 * if a is a constaant : this function returns 1, otherwise returns 0.
 */
static inline int isConstant_spX (const duspoly_t* a)
{
    if (isZero_spX (a)) {
        return 0;
    }
    if (POLY_LT(a) == 0) {
        return 1;
    }
    return 0;
}

/**
 * isConstant function 
 * if a = 0 + X : this function returns 1, otherwise returns 0.
 */
static inline int isX_spX (const duspoly_t* a)
{    
    if (isZero_spX (a) || POLY_LT(a) != 1) {
        return 0;
    }

    if (a->elems[0] == 0 && a->elems[1] == 1) {
        return 1;
    }
    return 0;
}

/*Supposed that a is in Montgomery form and looking for convert_in_montgomery(1)*X^1 */
static inline int isXInForm_spX (const duspoly_t* a, const Prime_ptr* Pptr)
{    
    if (isZero_spX (a) || POLY_LT(a) != 1) {
        return 0;
    }

    if (a->elems[0] == 0 && a->elems[1] == smallprimefield_convert_in (1, Pptr)) {
        return 1;
    }
    return 0;
}

/** 
 * isEqual Function 
 * if a == b then return 1, otherwise return 0.
 */
static inline int isEqual_spX (const duspoly_t* a, const duspoly_t* b)
{
    if (isZero_spX (a) && isZero_spX (b)) {
        return 1;
    }
    
    if (a == NULL) {
        if (b == NULL) {
            return 1;
        } else {
            return 0;
        }
    } else if (b == NULL) {
        return 0;
    }

    if (POLY_LT(a) != POLY_LT(b)) {
        return 0;
    }
    
    int isCmp = memcmp (a->elems, b->elems, (POLY_LT(a)+1)*sizeof(elem_t)); 

    return !isCmp;
    
}

/**
 * Swap two polynomials (a,b) with their pointers
 * 
 */
static inline void swap_spX (duspoly_t** a, duspoly_t** b) 
{
    duspoly_t* t = *a;
    *a = *b;
    *b = t;
}


/** 
 * Reset leading term of a polynomial 
 * a->lt and a->alloc will update w.r.t the leading term of a
 */
static inline void normalize_spX (duspoly_t** a)
{
    if (isZero_spX(*a)) {
        return;
    }
    
    int n = POLY_LT(*a);
    elem_t* elems = (*a)->elems;
    
    while (n >= 0 && elems[n] == 0) {
        n--;
    }
    
    if (n < 0) {
        freePolynomial_spX (a);
        return;
    }
    
    if (POLY_ALLOC(*a) - n > 16) {
        elem_t* Elems = (elem_t*) realloc (elems, sizeof(elem_t)*(n+1));
        if (!Elems) {
            fprintf (stderr, "DUSP Error: the reallocation is failed in normalize_spX!\n");
            exit (1);
        }
        // free (elems); 
        (*a)->elems = Elems;
        POLY_ALLOC (*a) = n+1;
    }

    POLY_LT(*a) = n;
}

/**
 * Set variable lt of input polynomial
 * if setAlloc != 0, then realloc polynomial w.r.t the lt.
 */
static inline void setLT_spX (duspoly_t** a, int setAlloc) 
{
    if (isZero_spX (*a)) {
        return;
    }

    polysize_t n = POLY_ALLOC(*a);

    while (n > 0 && (*a)->elems[n-1] == 0) {
        n--;
    }

    POLY_LT (*a) = n-1;

    if (setAlloc) {
        normalize_spX (a);
    }
}

/** 
 * Reallocation Function
 * resize polynomial a to sz 
 */
static inline void reallocatePolynomial_spX (duspoly_t** a, polysize_t sz)
{

    if (sz == 0) {
        freePolynomial_spX (a);
        *a = NULL;
        return;
    }

    if (*a != NULL && POLY_ALLOC(*a) == sz) {
        return;
    }

   (*a)->elems = (elem_t*) realloc ((*a)->elems, sz*sizeof(elem_t));

    for (polysize_t i = POLY_LT(*a)+1; i < sz; i++) {
        (*a)->elems[i] = 0;
    }

    POLY_ALLOC (*a) = sz;

    if (POLY_LT(*a)+1 > sz) {
        setLT_spX (a, 0);
    }
}


/**
 * Set an array of coefficients 
 * a, will be a polynomial of size sz, with coefficient set coefs
 */
void setCoefs_spX (duspoly_t** a, elem_t* coefs, polysize_t sz, const Prime_ptr* Pptr);

/*Supposed the coefs is already in montgomery form and the function doesnt convert them*/
void setCoefsInForm_spX (duspoly_t** a, elem_t* coefs, polysize_t sz);

/** 
 * Get coefficients of a polynomial as an C array
 * Return a->elems 
 */ 
elem_t* getCoefs_spX (const duspoly_t* a, const Prime_ptr* Pptr);

/*Supposed that a is in Montgomery form and return a without converting.*/
elem_t* getCoefsInForm_spX (const duspoly_t* a);

elem_t* getCoefsInForm_spX_inp (duspoly_t* a);



/**********************
 * Convert Polynomial
 *********************/

/**
 * Convert coefficients of a polynomial to the montgomery form
 * Return a converted version of a.
 */
duspoly_t* convertPolynomialToMontgomery_spX (const duspoly_t* a, const Prime_ptr* Pptr);
void convertPolynomialToMontgomery_spX_inp (duspoly_t** a, const Prime_ptr* Pptr);


/**
 * Convert coefficients of a polynomial to the montgomery form
 * Return a converted version of a.
 */
duspoly_t* convertPolynomialFromMontgomery_spX (const duspoly_t* a, const Prime_ptr* Pptr);
void convertPolynomialFromMontgomery_spX_inp (duspoly_t** a, const Prime_ptr* Pptr);


/*************************
 * Shift/Split Polynomial
 *************************/

/**
 * Right shift a polynomial
 * sa will be shifted version of polynomial a of size n.
 * Ex:  (X^{n} >> n) = 1
 */
void rightShiftPolynomial_spX (const duspoly_t* a , duspoly_t** sa, polysize_t n);
void rightShiftPolynomial_spX_inp (duspoly_t** a, polysize_t n);

/**
 * Left shift a polynomial
 * sa will be shifted version of polynomial a of size n.
 * Ex: (1 << n) = X^{n}
 */
void leftShiftPolynomial_spX (const duspoly_t* a , duspoly_t** sa, polysize_t n);

/**
 * Left shift polynomial b of size n, and add with polynomial a, in-place.
 */
void leftShiftAddPolynomials_spX_inp (duspoly_t** a, const duspoly_t* b,  polysize_t n, const Prime_ptr* Pptr);

/**
 * Left shift polynomial b of size n, and sub with polynomial a, in-place.
 */
void leftShiftSubPolynomials_spX_inp (duspoly_t** a, const duspoly_t* b,  polysize_t n, const Prime_ptr* Pptr);

/**
 * Left part of a polynomial
 * sa will be left part of polynomial a of size n
 * Ex: f = 1 + x^1 + 3x^2 + 5x^4 --> n < 1 : return f = 0;
 *                               --> n = 3 : return f = 1 + x^2 + 3x^2
 */
void leftSplitPolynomial_spX (const duspoly_t* a, duspoly_t** sa, polysize_t n);


/***********************************
 * Basic Polynoimal Func/Arithmetic 
 **********************************/

/**
 * Degree of Polynomial
 */
static inline polysize_t degPolynomial_spX (const duspoly_t* a)
{    
    if (a == NULL) {
        return 0;
    }
    
    return POLY_LT(a);
}

/**
 * idx-th coefficient of polynomial in Montgomery form
 */
static inline elem_t idxCoeffInForm_spX (const duspoly_t* a, polysize_t idx)
{
    if (a == NULL || POLY_LT(a) < idx || idx < 0) {
        return 0;
    }
    
    return a->elems[idx];
}

/**
 * Leading Coefficient of polynomial in Montgomery form
 */
static inline elem_t leadingCoeffInForm_spX (const duspoly_t* a)
{
    if (a == NULL) {
        return 0;
    }
    return a->elems[POLY_LT(a)];
}

/**
 * Constant Term of polynomial in Montgomery form
 */
static inline elem_t constTermInForm_spX (const duspoly_t* a)
{
    if (a == NULL) {
        return 0;
    }
    return a->elems[0];
}

/**
 * Addition 
 * Return the addition of polynomials a and b in c.
 */
void addPolynomials_spX (const duspoly_t* a, const duspoly_t* b, duspoly_t** c, const Prime_ptr* Pptr);
void addPolynomialsInForm_spX (const duspoly_t* a, const duspoly_t* b, duspoly_t** c, const Prime_ptr* Pptr);

// Addition in-place w.r.t the first input
void addPolynomials_spX_inp (duspoly_t** a, const duspoly_t* b, const Prime_ptr* Pptr);
void addPolynomialsInForm_spX_inp (duspoly_t** a, const duspoly_t* b, const Prime_ptr* Pptr);

/**
 * Subtraction 
 * Return the subtraction of polynomials a and b in c.
 */
void subPolynomials_spX (const duspoly_t* a, const duspoly_t* b, duspoly_t** c, const Prime_ptr* Pptr);
void subPolynomialsInForm_spX (const duspoly_t* a, const duspoly_t* b, duspoly_t** c, const Prime_ptr* Pptr);

// Subtraction in-place w.r.t the first input
void subPolynomials_spX_inp (duspoly_t** a, const duspoly_t* b, const Prime_ptr* Pptr);
void subPolynomialsInForm_spX_inp (duspoly_t** a, const duspoly_t* b, const Prime_ptr* Pptr);

/**
 * Negation 
 * Negate input polynomial, 
 * Note this algorithm works in-place
 */
void negPolynomial_spX (duspoly_t* a, const Prime_ptr* Pptr);
void negPolynomialInForm_spX (duspoly_t* a, const Prime_ptr* Pptr);

/**
 * Scalar Multiplication
 * Return c which is  c = b*a = b*(a_1, ..., a_n) = (b*a_1, ..., b*a_n)
 */
void scalarMulPolynomial_spX (const duspoly_t* a, elem_t b, duspoly_t** c, const Prime_ptr* Pptr);
void scalarMulPolynomialInForm_spX (const duspoly_t* a, elem_t b, duspoly_t** c, const Prime_ptr* Pptr);
void scalarMulPolynomialInForm_spX_inp (duspoly_t** a, elem_t b, const Prime_ptr* Pptr);

/**
 * Monic Polynomial 
 * ma will be monic polynomial of a, and lc is leading coefficient.
 */
void monicPolynomial_spX (const duspoly_t* a, duspoly_t** ma, elem_t* lc, const Prime_ptr* Pptr);
void monicPolynomialInForm_spX (const duspoly_t* a, duspoly_t** ma, elem_t* lc, const Prime_ptr* Pptr);
void monicPolynomialInForm_spX_inp (duspoly_t** a, elem_t* lc, const Prime_ptr* Pptr);

/**
 * Plain Multiplication
 * Return Multiplication of polynomials a and b, in c
 */
void plainMulPolynomials_spX (const duspoly_t* a, const duspoly_t* b, duspoly_t** c, const Prime_ptr* Pptr);
void plainMulPolynomialsInForm_spX (const duspoly_t* a, const duspoly_t* b, duspoly_t** c, const Prime_ptr* Pptr);

/**
 * Multiply many polynomials together into a single product.
 * Given n polynomials in polys, multiply them together and return 
 * their product.
 *
 * returns the procuct of all polynomials in poly.
 */
duspoly_t* plainMulManyPolynomialsInForm_spX(duspoly_t const*const* polys, int n, const Prime_ptr* Pptr);

/**
 * Karatsuba Multiplication
 * Return the multiplication of a and b in c where
 * n : a power of two and n > deg(a) && n > deg(b). 
 */	
void KaratsubaMulPolynomialsInForm_spX (const duspoly_t* a, const duspoly_t* b, duspoly_t** c, const Prime_ptr* Pptr);
void KaratsubaMulPolynomialsInForm_spX_inp (duspoly_t** a, const duspoly_t* b, const Prime_ptr* Pptr);
void KaratsubaSqrPolynomialsInForm_spX (const duspoly_t* a, duspoly_t** a2, const Prime_ptr* Pptr);

/**
 * 6-step FFT Based Multiplication 
 * @note works with furier primes only
 * @note needs to tuned parameter K 
 */ 
void fftMulPolynomialsInForm_spX (const duspoly_t* a, const duspoly_t* b, duspoly_t** c, const Prime_ptr* Pptr);


/**
 * Multiply the polynomial pointed to be a_ptr by the 
 * monic binomial (x - b), returning the product in place.
 *
 * @param a_ptr[in,out] the pointer to the polynomial to be multiplied
 *              and where the resulting product is stored.
 * @param b the constant term of the multiplier binomial.
 * @param Pptr the Prime_ptr describing the finite field.
 *
 */
void multiplyByBinomialInForm_spX_inp(duspoly_t** a_ptr, elem_t b, const Prime_ptr* Pptr);



/**
* Exponentiate Polynomials 
*
* Computes the power of a polynomial, a,  to the n modulus another polynomial, f.
* 
* @param a is the input polynomial
* @param n is the exponent to compute a**n
* @param f is the modulus polynomial to compute a**n (mod f)
* @return an is the a**n, or a**n (mod f) if f is defined.
*/
void exponentiatePolynomialInForm_spX (const duspoly_t* a, polysize_t n, duspoly_t** an, const Prime_ptr* Pptr);
void modExponentiatePolynomialInForm_spX (const duspoly_t* a, polysize_t n, const duspoly_t* f, duspoly_t** an, const Prime_ptr* Pptr);


/**
 * Square
 * Squaring the polynomial a, in a2 
 */
void plainSqrPolynomial_spX (const duspoly_t* a, duspoly_t** a2, const Prime_ptr* Pptr);
void plainSqrPolynomialInForm_spX (const duspoly_t* a, duspoly_t** a2, const Prime_ptr* Pptr);


/**
 * Plain Division 
 * Return the remainder (r) and quotients (q) of the divisision a and b. such that a = qb + r.
 */
void plainDivPolynomials_spX (const duspoly_t* a, const duspoly_t* b, duspoly_t** r, duspoly_t** q, const Prime_ptr* Pptr);
void plainDivPolynomialsInForm_spX (const duspoly_t* a, const duspoly_t* b, duspoly_t** r, duspoly_t** q, const Prime_ptr* Pptr);

// Plain Division in-place w.r.t the first input as returned remainder (a%b).
void plainDivPolynomials_spX_inp (duspoly_t** a, const duspoly_t* b, duspoly_t** q, const Prime_ptr* Pptr);
void plainDivPolynomialsInForm_spX_inp (duspoly_t** a, const duspoly_t* b, duspoly_t** q, const Prime_ptr* Pptr);

static inline void plainExactDivPolynomialsInForm_spX (const duspoly_t* a, const duspoly_t* b, duspoly_t** q, const Prime_ptr* Pptr) {
    plainDivPolynomialsInForm_spX(a, b, NULL, q, Pptr);
}

/**
 * Plain Remainder  
 * Return a%b in r.  
 */
void plainRemPolynomials_spX (const duspoly_t* a, const duspoly_t* b, duspoly_t** r, const Prime_ptr* Pptr);
void plainRemPolynomialsInForm_spX (const duspoly_t* a, const duspoly_t* b, duspoly_t** r, const Prime_ptr* Pptr);

// Plain Remainder in-place w.r.t the first input.
void plainRemPolynomials_spX_inp (duspoly_t** a, const duspoly_t* b, const Prime_ptr* Pptr);
void plainRemPolynomialsInForm_spX_inp (duspoly_t** a, const duspoly_t* b, const Prime_ptr* Pptr);

/**
 * Divide the polynomial (a) by a monic linear polynomial (x - b). 
 * This function doubles as an evaluation method to evaluate a(b)
 * where the returned remainder is a evaluated at b.
 * 
 * @param a the dividend
 * @param b the constant term of the divisor, (x-b);
 * @param[out] q a pointer to the resulting quotient; may be NULL;
 * @param[out] rem a pointer to the resulting remainder; may be NULL. 
 *
 * @return 1 if the division was exact, 
 *         0 if the division was not exact, 
 *        -1 if an error occurred (such as invalid parameters; 
 */
int divideByMonicLinearInForm_spX(const duspoly_t* a, elem_t b, duspoly_t** q, elem_t* rem, const Prime_ptr* Pptr);

/**
 * Plain GCD 
 * g will be gcd of a and b. (c = gcd(a,b))
 */
void plainGCD_spX (const duspoly_t* a, const duspoly_t* b, duspoly_t** g, const Prime_ptr* Pptr);
void plainGCDInForm_spX (const duspoly_t* a, const duspoly_t* b, duspoly_t** g, const Prime_ptr* Pptr);
void _plainGCDInForm_spX (const duspoly_t* a, const duspoly_t* b, duspoly_t** g, elem_t** lcrems, const Prime_ptr* Pptr);
void _plainGCDInFormAtMaxK_spX (const duspoly_t* a, const duspoly_t* b, polysize_t k, duspoly_t** rk1, duspoly_t** rk, elem_t** lcrems, const Prime_ptr* Pptr);


/**
 * Extended Euclidean Algorithm
 * g = gcd(a,b) = a*u + b*v.
 */
void plainExtGCD_spX (const duspoly_t* a, const duspoly_t* b, duspoly_t** u, duspoly_t** v, duspoly_t** g, const Prime_ptr* Pptr);
void plainExtGCDInForm_spX (const duspoly_t* a, const duspoly_t* b, duspoly_t** u, duspoly_t** v, duspoly_t** g, const Prime_ptr* Pptr);


/**
 * Extended Euclidean Algorithm
 * g = gcd(a,b) = a*u + b*v.
 */
void plainMulModPoly_spX (duspoly_t* a, duspoly_t* b, duspoly_t* mod, duspoly_t** c, const Prime_ptr* Pptr);
void plainMulModPolyInForm_spX (duspoly_t* a, duspoly_t* b, duspoly_t* mod, duspoly_t** c, const Prime_ptr* Pptr);


/**
 * Square polynomial modulus polynomial
 * a2 will be polynomial a, modulus mod. 
 */
void plainSqrModPoly_spX (duspoly_t* a, duspoly_t* mod, duspoly_t** a2, const Prime_ptr* Pptr);
void plainSqrModPolyInForm_spX (duspoly_t* a, duspoly_t* mod, duspoly_t** a2, const Prime_ptr* Pptr);


/**
 * A wrapper for fastest multiplication and squaring algorithm in DUSP
 */
void mulPolynomialsInForm_spX (const duspoly_t* a, const duspoly_t* b, duspoly_t** c, const Prime_ptr* Pptr);
void mulPolynomialsInForm_spX_inp (duspoly_t** a, const duspoly_t* b, const Prime_ptr* Pptr);
void sqrPolynomialInForm_spX (const duspoly_t* a, duspoly_t** a2, const Prime_ptr* Pptr);
duspoly_t* mulManyPolynomialsInForm_spX(duspoly_t const*const* polys, int n, const Prime_ptr* Pptr);


/**
 * Inverse of polynomial modulus polynomial
 * am will be inverse of polynomial a, modulus mod. (a*am = 1 mod) 
 */
void plainInvModPoly_spX (duspoly_t* a, duspoly_t* mod, duspoly_t** am, const Prime_ptr* Pptr);
void plainInvModPolyInForm_spX (duspoly_t* a, duspoly_t* mod, duspoly_t** am, const Prime_ptr* Pptr);


/**
 * isInvertible modulus polynomial
 * if gcd(a, mod) = 1, returns 1, otherwise 0.
 */
int isInvertibleModPoly_spX (duspoly_t* a, duspoly_t* mod, const Prime_ptr* Pptr);
int isInvertibleModPolyInForm_spX (duspoly_t* a, duspoly_t* mod, const Prime_ptr* Pptr);

/**
 * Derivative of polynomial
 * ap is the derivative of polynomial a.  
 */
void derivativePolyInForm_spX (duspoly_t* a, duspoly_t** ap, const Prime_ptr* Pptr);
void derivativePolyInForm_spX_inp (duspoly_t** a, const Prime_ptr* Pptr);

/*****************************
 * Evaluation / Interpolation  
 ****************************/

/**
 * Interpolation with a Long-Division Returned Polynomial 
 * 
 * @param a duspoly_t polynomial 
 * @param u evaluated point 
 * @param q will be a long-division polynomial of a/(x-u) mod pr
 * @param au will be the evaluation result
 * @param Pptr small prime pointer 
 * 
 */
void evalDivPolynomials_spX (const duspoly_t* a, const elem_t u, duspoly_t** q, elem_t* au, const Prime_ptr* Pptr);

/**
 * Lagrange-Basis Interpolation
 * 
 * @param t evaluation points of size n
 * @param n deg(a)
 * @param *a will be \prod (x-t[i]) mod Pptr->prime
 * @param Pptr small prime pointer 
 */
void lagrangeBasisInForm_spX (const elem_t* t, const polysize_t n, duspoly_t** a, const Prime_ptr* Pptr);

/*****************************
 ** Structured Rand Polynomial
 ****************************/
/**
 * Fully Random Polynomial in Montgomery form 
 * Given positive integer, n, and return a random polynomial of degree less than n.
 * Note this function never return NULL, unless n = 0.
 */
duspoly_t* randomPolynomialInForm_spX (polysize_t n, const Prime_ptr* Pptr);

duspoly_t* binPolynomial_spX (polysize_t n, const Prime_ptr* Pptr);
duspoly_t* binPolynomialInForm_spX (polysize_t n, const Prime_ptr* Pptr);

/********************************
 ** Power Series Based Functions 
 *******************************/

/**
 * Power Series Inversion using Newton Iteration 
 * Return the inverse of polynomial a in ai where l is the precision of the algorithm.
 * such that ai is the a^{-1} mod x^{l}. 
 * Note a(0) = 1 and l > deg(a). 
 */ 
void plainPSInversionInForm_spX (const duspoly_t* a, duspoly_t** ai, polysize_t l, const Prime_ptr* Pptr);

/**
 * Reverse Polynomial
 * Return the reverse of polynomial a in ra, 
 * such that a(x) = x^{d} a(1/x)
 * Note d >= deg_a, otherwise it doesnt reverse the polynomial.
 */
void reversePolynomial_spX (const duspoly_t* a, duspoly_t** ra, polysize_t d, const Prime_ptr* Pptr);

/** 
 * Division Algorithm using Power Series Inversion 
 * Return the remainder (r) and quotients (q) of the divisision a and b. such that a = qb + r.
 */
void fastDivPolynomialInForm_wPSInv_spX (const duspoly_t* a, const duspoly_t* b, duspoly_t** r, duspoly_t** q, const Prime_ptr* Pptr);

/**
 * isDividable
 * if a%b = 0 returns 1, otherwise 0.
 */
int isDividablePolys_spX (const duspoly_t* a, const duspoly_t* b, const Prime_ptr* Pptr);
int isDividablePolysQuoInForm_spX (const duspoly_t* a, const duspoly_t* b, duspoly_t** q, const Prime_ptr* Pptr);

/**
 * Wrapper functions for Div/Rem
 */
void divPolynomialsInForm_spX (const duspoly_t* a, const duspoly_t* b, duspoly_t** r, duspoly_t** q, const Prime_ptr* Pptr);
void divPolynomialsInForm_spX_inp (duspoly_t** a, const duspoly_t* b, duspoly_t** q, const Prime_ptr* Pptr);
void remPolynomialsInForm_spX (const duspoly_t* a, const duspoly_t* b, duspoly_t** r, const Prime_ptr* Pptr);
void remPolynomialsInForm_spX_inp (duspoly_t** a, const duspoly_t* b, const Prime_ptr* Pptr);

/****************************
 ** Half-GCD Based Functions
 ****************************/

//A Matix 2*2 of polynomials known as Mat4
typedef struct dusmat4 {
	duspoly_t* polys[4]; // M(0,0) = polys[0]  |  M(0,1) = polys[1]
                         // M(1,0) = polys[2]  |  M(1,1) = polys[3]
} dusmat4_t;

static inline void freeMat4_spX (dusmat4_t** A)
{
    if (*A == NULL) {
		return;
    }
    
    freePolynomial_spX (&(*A)->polys[0]);
    freePolynomial_spX (&(*A)->polys[1]);
    freePolynomial_spX (&(*A)->polys[2]);
    freePolynomial_spX (&(*A)->polys[3]);
    free (*A);

    *A = NULL;
}

static inline polysize_t Log2_spX (polysize_t n) 
{
    return (n > 1) ? 1 + Log2_spX (n>>1) : 0;
}

static inline polysize_t LogCeiling_spX (polysize_t n) 
{
    if (n == 0) {
        return 0;
    } else if (n == 1) {
        return 1;
    }

    polysize_t p = 0; // power
    polysize_t t = n; // tmp
 
    while (t) {
        t >>= 1;
        p +=  1;
    }

    if ((n << 1) == (1 << p)) {
        p -= 1;
    }

    return p;
}
	
/** 
 * A boolean algorithm to return the equality of two Matrices A and B.
 */
static inline int isEqualMat4_spX (dusmat4_t* A, dusmat4_t* B)
{
    if (A == NULL) {
        if (B == NULL) {
            return 1;
        }
        return 0;
    }
    
    for (int i = 0; i < 4; ++i) {
        if (!isEqual_spX(A->polys[i], B->polys[i])) {
            return 0;
        }
    }
    return 1;
}

/** 
 * A boolean algorithm to return the equality of polynomial A and identity 2*2 matrix.
 */
static inline int isIdentityMat4InForm_spX (dusmat4_t* A, const Prime_ptr* Pptr) 
{
    if (A == NULL) {
        return 0;
    }

    if (!isZero_spX (A->polys[1]) || 
        !isZero_spX (A->polys[2])) {
        return 0;
    }

    duspoly_t* id = constPolynomial_spX (1, Pptr);

    if (!isEqual_spX (A->polys[0], id) || 
        !isEqual_spX (A->polys[3], id)) {

        freePolynomial_spX (&id);
        return 0;
    }

    freePolynomial_spX (&id);

    return 1;
}

/**
 * Make an Identity matrix 
 */
static inline void identityMat4_spX (dusmat4_t** A, const Prime_ptr* Pptr)
{
    dusmat4_t* Id = (dusmat4_t*) malloc (sizeof (dusmat4_t));
    Id->polys[0] = constPolynomialInForm_spX (1, Pptr);
    Id->polys[1] = NULL;
    Id->polys[2] = NULL;
    Id->polys[3] = constPolynomialInForm_spX (1, Pptr);
    
    *A = Id;
}

static inline void identityMat4InForm_spX (dusmat4_t** A, const Prime_ptr* Pptr)
{
    dusmat4_t* Id = (dusmat4_t*) malloc (sizeof (dusmat4_t));
    Id->polys[0] = constPolynomial_spX (1, Pptr);
    Id->polys[1] = NULL;
    Id->polys[2] = NULL;
    Id->polys[3] = constPolynomial_spX (1, Pptr);
    
    *A = Id;
}

/**
 * Get the specific polynomial from matrix A
 * Return A[i, j].
 * Note i = 0, or 1 and j = 0, or 1. 
 */	
duspoly_t* getPolyFromMat4InForm_spX (dusmat4_t* A, polysize_t i,polysize_t j);
duspoly_t* getPolyFromMat4InForm_spX_inp (dusmat4_t* A, polysize_t i,polysize_t j);

/**
 * Set the polynomial, a, in matrix A such that A[i, j] = a 
 * Note i = 0, or 1 and j = 0, or 1. 
 */	
void setPolyToMat4InForm_spX (dusmat4_t** A, duspoly_t* a, polysize_t i, polysize_t j);

/**
 * Multiplication of matrix, M, and vector of polynomials [U; V]
 * the output is [ U ] =  M * [ U ] = [ M00*U + M01*V ]
 *               [ V ]        [ V ]   [ M10*U + M11*V ]
 * Note this works in-place.
 */
void mulMat4ToVec_spX_inp (dusmat4_t* M, duspoly_t** u, duspoly_t** v, const Prime_ptr* Pptr);
void mulMat4ToVecInForm_spX_inp (dusmat4_t* M, duspoly_t** u, duspoly_t** v, const Prime_ptr* Pptr);

/**
 * Multiply two Matrices A and B which is in-place such that B = A*B
 * Note A will be free at the end.
 */
void mulMat4ToMat4_spX_inp_inp (dusmat4_t* A, dusmat4_t** B, const Prime_ptr* Pptr);
void mulMat4ToMat4InForm_spX_inp_inp (dusmat4_t* A, dusmat4_t** B, const Prime_ptr* Pptr);

// Note this works in-place over B, but A will not be free at the end.
void mulMat4ToMat4InForm_spX_inp (dusmat4_t* A, dusmat4_t* B, dusmat4_t** C, const Prime_ptr* Pptr);

/**
 * Half-GCD Matrix (MCA algorithm using the NTL_HGCD implementation tricks) 
 * Given polynomial a and b, such that deg(a) >= deg(b)
 * Return a 2 * 2 matrix M which redues gcd(a, b) to gcd(c, d) 
 * where deg(c) >= (deg(a))/2 > deg(d) and M * [a; b] = [c; d].
 */
void iterHalfGCDMatrixInForm_spX (duspoly_t* a, duspoly_t* b, dusmat4_t** M, long d, const Prime_ptr* Pptr);   
void iterHalfGCDMatrixInForm_spX_inp (duspoly_t** a, duspoly_t** b, dusmat4_t** M, long d, const Prime_ptr* Pptr);
void _iterHalfGCDMatrixInForm_spX_inp (duspoly_t** a, duspoly_t** b, dusmat4_t** M, long d, elem_t** lcrems, const Prime_ptr* Pptr);
void _iterHalfGCDMatrixInFormAtMaxK_spX_inp (duspoly_t** a, duspoly_t** b, dusmat4_t** M, long d, polysize_t k, elem_t** lcrems, const Prime_ptr* Pptr); 

void halfGCDMatrixInForm_spX (duspoly_t* a, duspoly_t* b, dusmat4_t** M, long d, const Prime_ptr* Pptr);
void _halfGCDMatrixInForm_spX (duspoly_t* a, duspoly_t* b, dusmat4_t** M, long d, elem_t** lcrems, const Prime_ptr* Pptr);

/**
 * Extended Half-GCD Matrix (MCA algorithm using the NTL_HGCD implementation tricks) 
 * Given polynomial a and b, such that deg(a) >= deg(b)
 * Return a 2 * 2 matrix M in which M[0,0]*a + M[0,1]*b = g  
 * Note g is not always monic
 */
void extHalfGCDMatrixInForm_spX (duspoly_t* a, duspoly_t* b, dusmat4_t** M, long d, const Prime_ptr* Pptr);
void extHalfGCDMatrixInForm_spX_inp (duspoly_t** a, duspoly_t** b, dusmat4_t** M, long d, const Prime_ptr* Pptr);

/**
 * Helper functions used in GCD, extGCD, hgcdResultant and hgcdSubresultant algorithms. 
 */
void halfGCDInForm_spX (duspoly_t* a, duspoly_t* b, duspoly_t** ap, duspoly_t** bp, const Prime_ptr* Pptr);
void halfGCDInForm_spX_inp (duspoly_t** a, duspoly_t** b, const Prime_ptr* Pptr);
void _halfGCDInForm_spX_inp (duspoly_t** a, duspoly_t** b, elem_t** lcrems, const Prime_ptr* Pptr); 
void _halfGCDInFormAtMaxK_spX_inp (duspoly_t** a, duspoly_t** b, polysize_t k, elem_t** lcrems, const Prime_ptr* Pptr);
void _HGCDInFormAtMaxK_spX (const duspoly_t* a, const duspoly_t* b, polysize_t k, duspoly_t** rk1, duspoly_t** rk, elem_t** lcrems, const Prime_ptr* Pptr);



/**
 * Yap Half-GCD algorithm (see Fundamental Problems of Algorithmic Algebra, C.K. Yap) 
 */
void iterYapHalfGCDMatrixInForm_spX (duspoly_t* a, duspoly_t* b, dusmat4_t** M, const Prime_ptr* Pptr);
void YapHalfGCDMatrixInForm_spX (duspoly_t* a, duspoly_t* b, dusmat4_t** M, const Prime_ptr* Pptr);
void YapHalfGCDMatrixInForm_wPSInv_spX (duspoly_t* a, duspoly_t* b, dusmat4_t** M, const Prime_ptr* Pptr);
void GCDMatrixInForm_wHGCD_spX (duspoly_t* a, duspoly_t* b, dusmat4_t** M, const Prime_ptr* Pptr);
void GCDInForm_wHGCD_spX (duspoly_t* a, duspoly_t* b, duspoly_t** g, const Prime_ptr* Pptr);
void ExtGCDInForm_wHGCD_spX (duspoly_t* a, duspoly_t* b, duspoly_t** u, duspoly_t** v, duspoly_t** g, const Prime_ptr* Pptr);



/****************************
 ** Resultant and GCD 
 ****************************/

typedef struct duspolys {
	duspoly_t* poly;
	struct duspolys* next;
} duspolys_t;
	

static inline void freePolys_spX (duspolys_t* aa) {
    if (aa == NULL) {
		return;
    }
	
    duspolys_t* cur = aa;
    while (cur != NULL) {
		if (cur->poly != NULL) {
			freePolynomial_spX (&(cur->poly));
		}
		cur = cur->next;
    }
}

typedef struct duspolysA {
	duspoly_t** polys;
	int size;
} duspolysA_t;

static inline duspolysA_t* makePolysA_spX (int sz) {
    if (sz < 1) {
        return NULL;
    }
    duspolysA_t* res = (duspolysA_t*) malloc (sizeof(duspolysA_t));
    res->polys = (duspoly_t**) malloc (sz * sizeof(duspoly_t*));
    for (int i=0; i < sz; i++) {
        res->polys[i] = NULL;
    }
    res->size = sz;

    return res;
} 

static inline duspolysA_t* makePolysAwithMaxAlloc_spX (int sz, polysize_t max_alloc) {
    if (sz < 1) {
        return NULL;
    }

    duspolysA_t* res = (duspolysA_t*) malloc (sizeof(duspolysA_t));
    res->polys = (duspoly_t**) malloc (sz * sizeof(duspoly_t*));
    for (int i=0; i < sz; i++) {
        res->polys[i] = makePolynomial_spX (max_alloc);
    }
    res->size = sz;

    return res;
} 

static inline void freePolysA_spX (duspolysA_t* aa) {
    if (aa == NULL) {
		return;
    }

    if (!aa->size) {
        free (aa);
        return;
    }
	
    for (int i = 0; i < aa->size; i++) {
        freePolynomial_spX (&aa->polys[i]);
    }
    free (aa->polys);
    free (aa);
}

/** 
 * Transpose Mat(size, max_deg+1) to  Mat(max_deg+1, size)
 * 
 * @param aa an array of polys (duspolysA_t)
 * @param max_deg maximum degree of polynomials in aa
 * @return aa_T 
 */ 
static inline duspolysA_t* transposePolysA_spX (duspolysA_t* aa, polysize_t max_deg)
{
    if (aa == NULL || aa->size == 0) {
        return NULL;
    }
    // aa: Mat(size, max_deg+1)
    int size = aa->size;
    polysize_t t;
    // aa_T : Mat(max_deg+1, size)
    duspolysA_t* aa_T = makePolysAwithMaxAlloc_spX (max_deg+1, size);
    for (int j=0; j<=max_deg; j++) { // traverse monomials
        for (int i=0; i<size; i++) { // traverse polynomials 
            aa_T->polys[j]->elems[i] = (aa->polys[i] != NULL && aa->polys[i]->lt >= j) ? 
                                        aa->polys[i]->elems[j] : 0;
        }
        aa_T->polys[j]->lt = size-1;

        if (!aa_T->polys[j]->elems[size-1]) {
            t = size - 2;
            while (!aa_T->polys[j]->elems[t]) {
                t--;
            }
            if (t < 0) {
                freePolynomial_spX (&aa_T->polys[j]);
            } else {
                aa_T->polys[j]->lt = t;
                // TODO: need reallocation?
            }
        }
    }

    return aa_T;
}

/**
 * Equality Check for duspolysA_t
 * 
 * @param aa an array of polynomials 
 * @param bb an array of polynomials
 * @return 1 if aa == bb, otherwise 0.
 */
static inline int isPolysAEqual_spX (duspolysA_t* aa, duspolysA_t* bb) 
{
    int printOption = 1;

    if (aa == NULL) {
        if (bb == NULL) {
            return 1;
        } else {
if (printOption) {
            fprintf (stderr, "DUSP: In isPolysAEqual, aa == NULL and bb != NULL\n");
}
            return 0;
        }
    } else if (bb == NULL) {
if (printOption) {
        fprintf (stderr, "DUSP: In isPolysAEqual, aa != NULL and bb == NULL\n");
}
        return 0;
    }

    if (aa->size != bb->size) {
if (printOption) { 
        fprintf (stderr, "DUSP: In isPolysAEqual, aa->size=%d and bb->size=%d\n", aa->size, bb->size);
}
        return 0;
    }

    for (int i=0; i<aa->size; i++) {
        if (!isEqual_spX (aa->polys[i], bb->polys[i])) {
if (printOption) {
        fprintf (stderr, "DUSP: In isPolysAEqual, aa->polys[%d] != bb->polys[%d]\n", i,i);
}
            return 0;
        }
    }

    return 1;
}
	
/**
 * GCD Algorithm using HalfGCD (MCA) and plainGCD 
 * This algorithm returns monic polynomial g = gcd(a,b).
 * Note the threshold for plainGCD is defined:  PLAINGCD_CROSSOVER
 */
void GCDInForm_spX (const duspoly_t* a, const duspoly_t* b, duspoly_t** g, const Prime_ptr* Pptr);
void _GCDInForm_spX (const duspoly_t* a, const duspoly_t* b, duspoly_t** g, elem_t** lcrems, const Prime_ptr* Pptr);

/**
 * Extened GCD Algorithm using extHalfGCD and plainExtGCD
 * This algorithm returns monic polynomial g = gcd(a,b), 
 * and tuple (u,v), so that a*u + b*v = g.
 * Note the threshold for plainGCD is defined:  PLAINGCD_CROSSOVER
 */
void extGCDInForm_spX (const duspoly_t* a, const duspoly_t* b, duspoly_t** u, duspoly_t** v, duspoly_t** g, const Prime_ptr* Pptr);

/**
 * Sylvester Resultant Algorithm
 * res = resultant (a,b)
 */
void sylvResultantInForm_spX (const duspoly_t* a, const duspoly_t* b, duspoly_t** res, const Prime_ptr* Pptr);
void hgcdResultantInForm_spX (const duspoly_t* a, const duspoly_t* b, duspoly_t** res, const Prime_ptr* Pptr);

/**
 * Sylvester Subresultant Algorithm
 * sibres is an array of subresultant chain for polyanomials a and b, and sz is its size.
 */
void brownSubResultantInForm_spX (duspoly_t* a, duspoly_t* b, duspolysA_t** subresA, polysize_t* sz, const Prime_ptr* Pptr);
void sylvSubResultantInForm_spX (duspoly_t* a, duspoly_t* b, duspolys_t** subres, polysize_t* sz, const Prime_ptr* Pptr);
void sylvSubResultantInFormA_spX (duspoly_t* a, duspoly_t* b, duspolysA_t** subresA, polysize_t* sz, const Prime_ptr* Pptr);
void _sylvSubResultantInFormA_spX (duspoly_t* a, duspoly_t* b, duspolysA_t** subresA, const polysize_t first_idx, polysize_t* sz, const Prime_ptr* Pptr);
void hgcdSubResultantInFormA_spX (duspoly_t* a, duspoly_t* b, polysize_t k, duspolysA_t** ksubresA, polysize_t* sz, const Prime_ptr* Pptr);



/****************************
 ** HGCD-based Subresultant (OLD/TEST Codes)
 ****************************/
	
/** 
 * Helper functions for kth-subresultant algorithm based on Yap and MCA Half-GCD
 * Note MCA Half-GCD is better in timing, so, the resutlant and subresultant algorithms are using this algorithm now.
 */
void YapHGCD_BaseCaseInFrom_spX (duspoly_t* a, duspoly_t* b, dusmat4_t** M, duspoly_t*** Quo, elem_t** LC, polysize_t** DD, polysize_t* len, const Prime_ptr* Pptr);
void YapHGCD_TotalCaseInFrom_spX (duspoly_t* a, duspoly_t* b, dusmat4_t** M, duspoly_t*** Quo, elem_t** LC, polysize_t** DD, polysize_t* len, const Prime_ptr* Pptr);

void extYapHGCD_BaseCaseInFrom_spX (duspoly_t* a, duspoly_t* b, polysize_t k, dusmat4_t** M, duspoly_t*** Quo, elem_t** LC, polysize_t** DD, polysize_t* len, const Prime_ptr* Pptr);
void extYapHGCD_TotalCaseInFrom_spX (duspoly_t* a, duspoly_t* b, polysize_t k, dusmat4_t** M, duspoly_t*** Quo, elem_t** LC, polysize_t** DD, polysize_t* len, const Prime_ptr* Pptr);
		
void MCAHalfGCDInForm_spX (duspoly_t* a, duspoly_t* b, polysize_t mca_k, dusmat4_t** M, duspoly_t*** Quo, elem_t** LC, polysize_t** DD, polysize_t* len, polysize_t* h, const Prime_ptr* Pptr);
void MCAHGCD_BaseCaseInForm_spX (duspoly_t* a, duspoly_t* b, polysize_t k, polysize_t mca_k, dusmat4_t** M, duspoly_t*** Quo, elem_t** LC, polysize_t** DD, polysize_t* len, polysize_t* h, const Prime_ptr* Pptr);
void MCAHGCD_TotalCaseInFrom_spX (duspoly_t* a, duspoly_t* b, polysize_t k, dusmat4_t** M, duspoly_t*** Quo, elem_t** LC, polysize_t** DD, polysize_t* len, const Prime_ptr* Pptr);


void quotientsToRemainders_spX (duspoly_t** Quo, duspoly_t*** Rem, polysize_t qlen, const Prime_ptr* Pptr);
void resultantYapHGCDInForm_spX (duspoly_t* a, duspoly_t* b, duspoly_t** res, const Prime_ptr* Pptr);
void subResultantYapHGCDInForm_spX (duspoly_t* a, duspoly_t* b, duspolys_t** subres, polysize_t* sz, const Prime_ptr* Pptr);

void kSubResultantYapHGCDInForm_spX (duspoly_t* a, duspoly_t* b, polysize_t k, duspoly_t** ksubres, const Prime_ptr* Pptr);
void kSubResultantMCAHGCDInForm_spX (duspoly_t* a, duspoly_t* b, polysize_t k, duspoly_t** ksubres, const Prime_ptr* Pptr);


/****************************
 ** Polynomial Factorization
 ****************************/

typedef struct factors {
    duspoly_t** polys;
    polysize_t* exps;
    polysize_t alloc;
} factors_t;

typedef struct factsll {
    polysize_t exp;
    duspoly_t* poly;
    struct factsll* next;
} factsll_t;

// See DUSP_Support_Factoring.h


/******************************************
 * Bivariate Polynomials over Small Primes
 *****************************************/

/** 
 * Dense Bivariate Integer Polynomial C struct Z_p[x > y].
 * Dense array of duspoly_t polynomials over Z_p[y], coefsY.
 * partial leading term based on X, ltX and coefsY allocation size, allocX. 
 */
typedef struct {
    polysize_t ltX;
    polysize_t allocX;
    duspoly_t** coefsY;
} dbspoly_t;

/**
 * Allocate a dbspoly with alloc number of coefficients. 
 * i.e., alloc = partial maximum degree - 1.
 * returns the newly allocated dbpoly.
 */
static inline dbspoly_t* makePolynomial_spXY (int allocX) {
	if (allocX <= 0) {
		allocX = 1; 
	}
	dbspoly_t* p = (dbspoly_t*) malloc(sizeof(dbspoly_t));
	p->coefsY = (duspoly_t**) malloc (allocX*sizeof(duspoly_t*));
	for (polysize_t i = 0; i < allocX; i++) {
		p->coefsY[i] = NULL;
	}
	p->allocX = allocX;
	p->ltX = 0;
	return p;
}

/**
 * Make a constant dbspoly_t polynomial
 * 
 * @param allocX allocation size
 * @param z NOT-in-form coef (elem_t)
 * @return 
 */
static inline dbspoly_t* constPolynomial_spXY (int allocX, elem_t z, const Prime_ptr* Pptr) {
	dbspoly_t* ret = makePolynomial_spXY (allocX);
	ret->coefsY[0] = constPolynomial_spX (z, Pptr);
	return ret;
}


/**
 * Make a constant dbspoly_t polynomial
 * 
 * @param allocX allocation size
 * @param z InForm coef (elem_t)
 * @return 
 */
static inline dbspoly_t* constPolynomialInForm_spXY (int allocX, elem_t z, const Prime_ptr* Pptr) {
	dbspoly_t* ret = makePolynomial_spXY (allocX);
	ret->coefsY[0] = constPolynomialInForm_spX (z, Pptr);
	return ret;
}

/**
 * Free a dbspoly_t.
 * @note
 */
static inline void freePolynomial_spXY (dbspoly_t* p) {
	if (p != NULL) {
		polysize_t lt = p->ltX;
		for (polysize_t i = 0; i <= lt; i++) {
			freePolynomial_spX (&(p->coefsY[i]));
		}
		free(p->coefsY);
		free(p);
	}
}

/**
 * Resize a dbspoly polynomial 
 * @note this works in-place
 * 
 * @param p dbspoly polynomial 
 * @param allocSize new allocation size
 */
static inline void resizePolynomial_spXY (dbspoly_t* p, polysize_t allocSize) {
	if (p == NULL) {
		return;
	}

	if (allocSize == p->allocX) {
		return;
	}

	if (allocSize <= p->ltX) {
		for (polysize_t i = p->ltX; i >= allocSize; --i) {
			freePolynomial_spX (&(p->coefsY[i]));
		}
		p->ltX = allocSize-1;
	}
	p->coefsY = (duspoly_t**) realloc (p->coefsY, sizeof(duspoly_t*)*allocSize);
	for (polysize_t i = p->ltX+1; i < allocSize; i++) {
		p->coefsY[i] = NULL;
	}
	p->allocX = allocSize;
}

/**
 * (distributed) Bivariate Subresultant Chain Data-Type for small prime 
 * @note each subresultant-chain is stored in one C-array
 * @note using distributed-data-type led to a better performance for modularSubres over Z[x>y].
 */
typedef struct {
    polysize_t n; // Number of polynomials in a subresultant-chain, at least 2
    polysize_t** deg; // Partial degree of each polynomial (bi-var: deg[2])
    polysize_t* size; // size of each polynomial array
    elem_t** coefs; // coefficients of polynomials 
} biSubresPr_t;

static inline biSubresPr_t* makeBiSubresultantInForm_spX (polysize_t n) {
    if (n < 0) {
        return NULL;
    }
    biSubresPr_t* res = (biSubresPr_t*) malloc (sizeof(biSubresPr_t));
    res->n = n;
    if (n > 0) {
        if (n > 1) {
            res->deg = (polysize_t**) malloc (n*sizeof(polysize_t*));
            res->coefs = (elem_t**) malloc (n*sizeof(elem_t*)); 
        }
        res->size = (polysize_t*) calloc (n, sizeof (polysize_t));
        for (polysize_t i = 0; i < n; i++) {
            res->deg[i] = (polysize_t*) calloc (2, sizeof(polysize_t));  
        }
    } else {
        res->n = 0;
        res->deg = NULL;
        res->size = NULL;
        res->coefs = NULL;
    }
    return res;
}

static inline void freeBiSubresultantInForm_spX (biSubresPr_t* s) {
    if (s == NULL) {
        return;
    }
    if (s->n) {
        for (polysize_t i = 0; i < s->n; i++) {
            free (s->deg[i]);
            if (s->size[i])
                free (s->coefs[i]);
        }
        free (s->deg);
        free (s->size);
        free (s->coefs);
    }
    free (s);
}



/**
 * Compute Partial Subresultant Chains w.r.t y in Z[x>y]
 * @note This makes use of lagrange algorithm for interpolation 
 * @note There are independent for loops (cilk_for hopes!)
 * 
 * @param pSubres partial subresultant chains s.t. every poly in this sequence obtained after specializing x in Z[x>y].
 * @param XYsubres returned bivariate subresultant chains over Z[x>y] modulus pr
 * @param t evaluation points 
 * @param n size of the sequence, that is, the degree+1 of the targeted subresultant
 * @param alpha block size 
 * @param Pptr small prime pointer
 */
void _interpolatePartialSubResInForm_spXY (duspolysA_t** pSubres, biSubresPr_t* XYsubres, elem_t* t, polysize_t n, polysize_t alpha, const Prime_ptr* Pptr);


/**
 * Bivariate Subresultant Chain  
 * 
 * @param g a bivariate polynomial (dbspoly_t) 
 * @param gpd 2Dim partial degrees of polynomial g  
 * @param f a bivariate polynomial (dbspoly_t) s.t. deg(g, x) > deg(f, x)
 * @param fpd 2Dim partial degrees of polynomial f
 * @param subres returned subresultant chain modular pr (BiSybresPr_t)
 * @param Pptr small prime pointer
 */
void biSylvSubResultantInForm_spX (elem_t* g, polysize_t* gpd, elem_t* f, polysize_t* fpd, biSubresPr_t** subres, const Prime_ptr* Pptr);

#ifdef __cplusplus
}
#endif

#endif
