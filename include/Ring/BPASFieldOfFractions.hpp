
#ifndef _BPAS_FIELD_OF_FRACTIONS_
#define _BPAS_FIELD_OF_FRACTIONS_

#include "BPASField.hpp"
#include "../Utils/TemplateHelpers.hpp"



/**
 * An abstract class defining the interface of a field of fractions.
 * The field is templated by a generic GCD domain.
 * The GCD domain must be a BPASGCDDomain.
 */
template <class Domain, class Derived>
class BPASFieldOfFractions : public virtual BPASField<Derived>, private Derived_from<Domain, BPASGCDDomain<Domain>> {
	public:

		/**
		 * Canonicalize this fraction, reducing terms as needed.
		 */
		virtual void canonicalize() = 0;

		/**
		 * Get the fraction's numerator.
		 *
		 * @return the numerator.
		 */
		virtual Domain numerator() const = 0;

		/**
		 * Get the fraction's denominator.
	 	 * @return the denominator
		 */
		virtual Domain denominator() const = 0;
};

#endif
