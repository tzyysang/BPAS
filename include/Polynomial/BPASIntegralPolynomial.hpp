


#ifndef _BPAS_INTEGRALPOLY_H_
#define _BPAS_INTEGRALPOLY_H_

#include "Utils/TemplateHelpers.hpp"


/**
 * An abstract class defining the interface of a polynomial ring which
 * is also an integral domain. E.g., a polynomial over an integral domain.
 *
 * This class is automatically determined to be a superclass of
 * a BPASPolynomial based on a template specialization of the Ring
 * parameter.
 */
template <class Ring, class Derived>
class BPASIntegralPolynomial : public virtual BPASBasePolynomial<Ring, Derived>,
							   public virtual BPASIntegralDomain<Derived>,
						       private Derived_from<Ring, BPASIntegralDomain<Ring>> {
	// virtual std::vector<Derived> squareFree() const = 0; //TODO
};



#endif
