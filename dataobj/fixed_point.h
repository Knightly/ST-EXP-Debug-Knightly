/*
 * Copyright (c) 2011 : Knightly
 *
 * This file is part of the Simutrans project under the artistic licence.
 * (see licence.txt)
 *
 * A fixed point data type template implemented with integer operations on the underlying integer representation
 */


#ifndef fixed_point_h
#define fixed_point_h


#include <type_traits>
using namespace std;

#include "../simtypes.h"


template<typename T /* underlying integer type */, sint32 P /* number of bits for precision */>
class fixed_point_t
{
	template<typename U, sint32 Q> friend class fixed_point_t;

public:
	/**
	 * For each bit reserved for precision, we need to reserve another bit to avoid precision overflow during multiplications and precision loss during divisions.
	 * Hence, if there are P precision bits, we need to reserve 2P bits. Then integer bits would be total number of bits less sign bit (if any) and 2P.
	 */
	static const sint32 PRECISION_BITS = P;
	static const sint32 INTEGER_BITS = (sizeof(T) * 8) - (is_signed<T>::value ? 1 : 0) - (PRECISION_BITS * 2);
	static const T PRECISION_FACTOR = ((T)1 << P);

	static_assert( is_integral<T>::value && !is_same<T, bool>::value && !is_const<T>::value , "Underlying type of fixed point type must be a non-const integer type" );
	static_assert( INTEGER_BITS >= 1 , "Integer bits must not be less than 1" );

	// check whether bit shifting has expected behaviour with respect to the underlying integer type or the compiler/platform
	static const bool LEFT_SHIFT = is_unsigned<T>::value || ( (-1) << 1 ) == (-2);
	static const bool RIGHT_SHIFT = is_unsigned<T>::value || ( (-1) >> 1 ) == (-1);

private:
	T raw_value;	// the underlying integer representation

	// select the appropriate implemenations for increasing and decreasing precision
	template<typename U> static typename enable_if<LEFT_SHIFT, U>::type
		inc_precision(const U value, const sint32 bits = PRECISION_BITS) { return value << bits; }
	template<typename U> static typename enable_if<!LEFT_SHIFT, U>::type
		inc_precision(const U value, const sint32 bits = PRECISION_BITS) { return value * ( (U)1 << bits ); }
	template<typename U> static typename enable_if<RIGHT_SHIFT, U>::type
		dec_precision(const U value, const sint32 bits = PRECISION_BITS) { return value >> bits; }
	template<typename U> static typename enable_if<!RIGHT_SHIFT, U>::type
		dec_precision(const U value, const sint32 bits = PRECISION_BITS) { return value>=0 ? value >> bits : ( value - (((U)1 << bits) - 1) ) / ((U)1 << bits); }

	static T square_root_internal(const T value)
	{
		// square root of 1 and 0
		if( value <= 1 ) {
			return value;
		}

		// determine the highest bit position in order to calculate an initial estimate
		sint32 bit_pos = (sizeof(T) * 8) - 1 - (is_signed<T>::value ? 1 : 0) - PRECISION_BITS;
		T bit_mask = (T)1 << bit_pos;
		for(  ; bit_mask  &&  !(value&bit_mask) ; bit_mask>>=1, --bit_pos );
		bit_pos = dec_precision(bit_pos - PRECISION_BITS, 1);
		T current_estimate = ( bit_pos >= 0 ? value >> bit_pos : value << -bit_pos );

		// refine the estimate in each iteration
		T last_estimate;
		do {
			last_estimate = current_estimate;
			current_estimate = ( ((value << PRECISION_BITS) / current_estimate) + current_estimate ) >> 1;
		} while( last_estimate > current_estimate );

		return current_estimate;
	}

	static T cubic_root_internal(const T value)
	{
		// cubic root of 1 and 0
		if( value <= 1 ) {
			return value;
		}

		// determine the highest bit position in order to calculate an initial estimate
		sint32 bit_pos = (sizeof(T) * 8) - 1 - (is_signed<T>::value ? 1 : 0) - PRECISION_BITS;
		T bit_mask = (T)1 << bit_pos;
		for(  ; bit_mask  &&  !(value&bit_mask) ; bit_mask>>=1, --bit_pos );
		bit_pos = inc_precision(bit_pos - PRECISION_BITS, 1);
		bit_pos = ( bit_pos >= 0 ? bit_pos / 3 : (bit_pos - 2) / 3 );
		T current_estimate = ( bit_pos >= 0 ? value >> bit_pos : value << -bit_pos );

		// refine the estimate in each iteration
		T last_estimate;
		do {
			last_estimate = current_estimate;
			current_estimate = ( ((value << PRECISION_BITS) / ((current_estimate * current_estimate) >> PRECISION_BITS)) + (current_estimate << 1) ) / 3;
		} while( last_estimate > current_estimate );

		return current_estimate;
	}

public:
	// constructors
	fixed_point_t() : raw_value(0) { }
	template<typename U> fixed_point_t(const U value, typename enable_if<is_integral<U>::value>::type* enable = 0)
		: raw_value( inc_precision( (T)value ) ) {  }
	template<typename U> fixed_point_t(const U value, typename enable_if<is_floating_point<U>::value>::type* enable = 0)
		: raw_value((T)(value * (U)PRECISION_FACTOR)) { }
	template<typename U, sint32 Q> explicit fixed_point_t(const fixed_point_t<U, Q> &other, typename enable_if<sizeof(T) >= sizeof(U)>::type* enable = 0)
		: raw_value( P >= Q ? inc_precision( (T)other.raw_value, P - Q ) : dec_precision( (T)other.raw_value, Q - P) ) { }
	template<typename U, sint32 Q> explicit fixed_point_t(const fixed_point_t<U, Q> &other, typename enable_if<sizeof(T) < sizeof(U)>::type* enable = 0)
		: raw_value((T)( P >= Q ? fixed_point_t<U, Q>::inc_precision( other.raw_value, P - Q ) : fixed_point_t<U, Q>::dec_precision( other.raw_value, Q - P) )) { }
	fixed_point_t(const fixed_point_t<T, P> &other) : raw_value(other.raw_value) { }
	fixed_point_t(const T numerator, const T denominator) : raw_value( inc_precision( numerator ) / denominator ) { }

	// conversion functions which need to be called explicitly
	template<typename U> typename enable_if<is_integral<U>::value && !is_same<U, bool>::value && !is_same<U, const bool>::value, U>::type
		to() const { return (U)dec_precision( raw_value ); }
	template<typename U> typename enable_if<is_floating_point<U>::value, U>::type
		to() const { return (U)raw_value / (U)PRECISION_FACTOR; }
	template<typename U> typename enable_if<is_same<U, bool>::value || is_same<U, const bool>::value, U>::type
		to() const { return (U)raw_value; }

	// assignment operator and arithmetic assignment operators
	fixed_point_t<T, P>& operator = (const fixed_point_t<T, P> &other) { raw_value = other.raw_value; return *this; }
	fixed_point_t<T, P>& operator += (const fixed_point_t<T, P> &other) { raw_value += other.raw_value; return *this; }
	fixed_point_t<T, P>& operator -= (const fixed_point_t<T, P> &other) { raw_value -= other.raw_value; return *this; }
	fixed_point_t<T, P>& operator *= (const fixed_point_t<T, P> &other) { raw_value = dec_precision( raw_value * other.raw_value ); return *this; }
	fixed_point_t<T, P>& operator /= (const fixed_point_t<T, P> &other) { raw_value = inc_precision( raw_value ) / other.raw_value; return *this; }

	// arithmetic operators defined in terms of their corresponding arithmetic assignment operators
	fixed_point_t<T, P> operator + (const fixed_point_t<T, P> &other) const { return ( fixed_point_t<T, P>(*this) += other ); }
	fixed_point_t<T, P> operator - (const fixed_point_t<T, P> &other) const { return ( fixed_point_t<T, P>(*this) -= other ); }
	fixed_point_t<T, P> operator * (const fixed_point_t<T, P> &other) const { return ( fixed_point_t<T, P>(*this) *= other ); }
	fixed_point_t<T, P> operator / (const fixed_point_t<T, P> &other) const { return ( fixed_point_t<T, P>(*this) /= other ); }

	// relational operators
	bool operator > (const fixed_point_t<T, P> &other) const { return raw_value > other.raw_value; }
	bool operator < (const fixed_point_t<T, P> &other) const { return raw_value < other.raw_value; }
	bool operator >= (const fixed_point_t<T, P> &other) const { return raw_value >= other.raw_value; }
	bool operator <= (const fixed_point_t<T, P> &other) const { return raw_value <= other.raw_value; }
	bool operator == (const fixed_point_t<T, P> &other) const { return raw_value == other.raw_value; }
	bool operator != (const fixed_point_t<T, P> &other) const { return raw_value != other.raw_value; }

	// various unary operators
	bool operator ! () const { return raw_value == 0; }
	fixed_point_t<T, P> operator + () const { return *this; }
	fixed_point_t<T, P> operator - () const { fixed_point_t<T, P> result(*this); result.raw_value = -result.raw_value; return result; }
	fixed_point_t<T, P>& operator ++ () { raw_value += PRECISION_FACTOR; return *this; }
	fixed_point_t<T, P>& operator -- () { raw_value -= PRECISION_FACTOR; return *this; }
	fixed_point_t<T, P> operator ++ (int) { fixed_point_t<T, P> original_value(*this); ++(*this); return original_value; }
	fixed_point_t<T, P> operator -- (int) { fixed_point_t<T, P> original_value(*this); --(*this); return original_value; }

	// utility functions
	fixed_point_t<T, P> square_root() const {
		fixed_point_t<T, P> result; result.raw_value = ( raw_value >= 0 ? square_root_internal(raw_value) : -PRECISION_FACTOR ); return result;
	}
	fixed_point_t<T, P> cubic_root() const {
		fixed_point_t<T, P> result; result.raw_value = ( raw_value >= 0 ? cubic_root_internal(raw_value) : -cubic_root_internal(-raw_value) ); return result;
	}
};


// global arithmetic operators
template<typename V, typename T, sint32 P> typename enable_if<is_arithmetic<V>::value, fixed_point_t<T, P> >::type
	operator + (const V value, const fixed_point_t<T, P> &fp_value) { return fixed_point_t<T, P>(value) += fp_value; }
template<typename V, typename T, sint32 P> typename enable_if<is_arithmetic<V>::value, fixed_point_t<T, P> >::type
	operator - (const V value, const fixed_point_t<T, P> &fp_value) { return fixed_point_t<T, P>(value) -= fp_value; }
template<typename V, typename T, sint32 P> typename enable_if<is_arithmetic<V>::value, fixed_point_t<T, P> >::type
	operator * (const V value, const fixed_point_t<T, P> &fp_value) { return fixed_point_t<T, P>(value) *= fp_value; }
template<typename V, typename T, sint32 P> typename enable_if<is_arithmetic<V>::value, fixed_point_t<T, P> >::type
	operator / (const V value, const fixed_point_t<T, P> &fp_value) { return fixed_point_t<T, P>(value) /= fp_value; }

// global relational operators
template<typename V, typename T, sint32 P> typename enable_if<is_arithmetic<V>::value, bool>::type
	operator > (const V value, const fixed_point_t<T, P> &fp_value) { return fixed_point_t<T, P>(value) > fp_value; }
template<typename V, typename T, sint32 P> typename enable_if<is_arithmetic<V>::value, bool>::type
	operator < (const V value, const fixed_point_t<T, P> &fp_value) { return fixed_point_t<T, P>(value) < fp_value; }
template<typename V, typename T, sint32 P> typename enable_if<is_arithmetic<V>::value, bool>::type
	operator >= (const V value, const fixed_point_t<T, P> &fp_value) { return fixed_point_t<T, P>(value) >= fp_value; }
template<typename V, typename T, sint32 P> typename enable_if<is_arithmetic<V>::value, bool>::type
	operator <= (const V value, const fixed_point_t<T, P> &fp_value) { return fixed_point_t<T, P>(value) <= fp_value; }
template<typename V, typename T, sint32 P> typename enable_if<is_arithmetic<V>::value, bool>::type
	operator == (const V value, const fixed_point_t<T, P> &fp_value) { return fixed_point_t<T, P>(value) == fp_value; }
template<typename V, typename T, sint32 P> typename enable_if<is_arithmetic<V>::value, bool>::type
	operator != (const V value, const fixed_point_t<T, P> &fp_value) { return fixed_point_t<T, P>(value) != fp_value; }


// type definition(s) for ease of use
typedef fixed_point_t<sint64, 16> fp16_t;
typedef fixed_point_t<sint64, 10> fp10_t;


#endif
