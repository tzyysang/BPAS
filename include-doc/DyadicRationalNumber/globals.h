#ifndef _QLOCAL_GLOBALS_H_
#define _QLOCAL_GLOBALS_H_

#include "rational.h"		// Alternative class for mpq_class
#include "../polynomial.h"

typedef mpz_class lfixz;
typedef mpq_class lfixq;
//typedef DyadicRationalNumber lfixq;


#define CPP11 0

#if __cplusplus==201103L
    #undef CPP11
    #define CPP11 1
    #include <regex>
#endif

#define MINUS "-"
#define STAR "*"

typedef struct {
        std::string poly;
        int exp;
}PolyAndExp;

class LminAndLmax{
        public:
            std::vector<std::pair<int, int> > Lmin;
            std::vector<std::pair<int, int> > Lmax;

            LminAndLmax(int na): Lmin(na, std::make_pair(-1, -1)), Lmax(na, std::make_pair(-1, -1)){}
};


/**
 * Univariate rational polynomial
 * Taylor Shift Divide & Conquer
 * Base Case: IncrementalCilkFor
 **/
#define URPTSDNCBASE 2048 // test for 2, 512, 1024, 2048

class RationalNumberTerm {
        public:
                lfixq coef;     // Coefficient
                int v;          // Number of variables
                int* degs;      // Degrees, in the ascending order of 
                                // the weight of variates
                                // Such that degs[0] < .. < degs[v-1]

                RationalNumberTerm () : coef(0), v(0) { }
                RationalNumberTerm (const RationalNumberTerm& b) : coef(b.coef), v(b.v) {
                        degs = new int[v];
                        std::copy(b.degs, b.degs+v, degs);
                }
                ~RationalNumberTerm () { if (v) { delete [] degs; } }

                inline RationalNumberTerm& operator= (RationalNumberTerm b) {
                        if (this != &b) {
                                coef = b.coef;
                                v = b.v;
                                degs = new int[v];
                                std::copy(b.degs, b.degs+v, degs);
                        }
                        return *this;
                }
};

class DesNode {
        public:
                int k;          // Depth of the node in the tree
                lfixz c;        // Length of the node in the tree
                DesNode* left;
                DesNode* right;
                DesNode* parent;
                DesNode* prev;
};

class DesTree {
        private:
                DesNode* root;

        public:
                DesTree();
                DesTree(int _k, lfixz _c);
                ~DesTree();

                DesNode* getNode();

                void addChildren(int k, lfixz c);
                DesNode* nextNode();
};

#endif
/* This file is part of the BPAS library http://www.bpaslib.org

    BPAS is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    BPAS is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with BPAS.  If not, see <http://www.gnu.org/licenses/>.

    Copyright:
        Mohammadali Asadi <masadi4@uwo.ca>
        Alexander Brandt <abrandt5@uwo.ca>
        Changbo Chen <changbo.chen@hotmail.com>
        Svyatoslav Covanov <svyatoslav.covanov@loria.fr>
        Farnam Mansouri <mansouri.farnam@gmail.com>
        Davood Mohajerani <mohajerani.d@gmail.com>
        Robert Moir <robert@moir.net>
        Marc Moreno Maza  <moreno@csd.uwo.ca>
        Delaram Talaashrafi <dtalaash@uwo.ca>
        Amha Tsegaye <atsegaye@uwo.ca>
        Linxiao Wang <lwang739@uwo.ca>
        Ning Xie <nxie6@csd.uwo.ca>
        Yuzhen Xie <yuzhenxie@yahoo.ca>

*/


