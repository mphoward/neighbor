// Copyright (c) 2018-2020, Michael P. Howard
// Copyright (c) 2021, Auburn University
// This file is released under the Modified BSD License.

// Maintainer: mphoward

/*!
 * \file upp11_config.h
 * \brief Defines macros for setting up and running unit tests in the upp11 unit testing framework.
 *
 * This file is partially based on the HOOMD-blue upp11_config.h for defining the main functions,
 * but is modified to include additional macros for convenient testing.
 */

#ifndef NEIGHBOR_TEST_UPP11_CONFIG_H_
#define NEIGHBOR_TEST_UPP11_CONFIG_H_

#include <upp11/upp11.h>

#include <cmath>
#include <string>

//! Macro to test if the difference between two floating-point values is within a tolerance
/*!
 * \param a First value to test
 * \param b Second value to test
 * \param eps Difference allowed between the two
 *
 * This assertion will pass if the difference between \a a and \a b is within a tolerance,
 * defined by \a eps times the smaller of the magnitude of \a a and \a b.
 *
 * \warning This assertion should not be used when one of the values should be zero. In that
 *          case, use UP_ASSERT_SMALL instead.
 */
#define UP_ASSERT_CLOSE(a,b,eps) \
upp11::TestCollection::getInstance().checkpoint(LOCATION, "UP_ASSERT_CLOSE"), \
upp11::TestAssert(LOCATION).assertTrue(std::abs((a)-(b)) <= (eps) * std::min(std::abs((float)a), std::abs((float)b)), \
                                       #a " (" + std::to_string(a) + ") close to " #b " (" + std::to_string(b) + ")")

//! Macro to test if a floating-point value is close to zero
/*!
 * \param a Value to test
 * \param eps Difference allowed from zero
 *
 * This assertion will pass if the absolute value of \a a is less than \a eps.
 */
#define UP_ASSERT_SMALL(a,eps) \
upp11::TestCollection::getInstance().checkpoint(LOCATION, "UP_ASSERT_SMALL"), \
upp11::TestAssert(LOCATION).assertTrue(std::abs(a) < (eps), #a " (" + std::to_string(a) + ") close to 0")

//! Macro to test if a value is greater than another
/*!
 * \param a First value to test
 * \param b Second value to test
 *
 * This assertion will pass if \a a > \b b.
 */
#define UP_ASSERT_GREATER(a,b) \
upp11::TestCollection::getInstance().checkpoint(LOCATION, "UP_ASSERT_GREATER"), \
upp11::TestAssert(LOCATION).assertTrue(a > b, #a " (" + std::to_string(a) + ") > " #b " (" + std::to_string(b) + ")")

//! Macro to test if a value is greater than or equal to another
/*!
 * \param a First value to test
 * \param b Second value to test
 *
 * This assertion will pass if \a a >= \b b.
 */
#define UP_ASSERT_GREATER_EQUAL(a,b) \
upp11::TestCollection::getInstance().checkpoint(LOCATION, "UP_ASSERT_GREATER_EQUAL"), \
upp11::TestAssert(LOCATION).assertTrue(a >= b, #a " (" + std::to_string(a) + ") >= " #b " (" + std::to_string(b) + ")")

//! Macro to test if a value is less than another
/*!
 * \param a First value to test
 * \param b Second value to test
 *
 * This assertion will pass if \a a < \b b.
 */
#define UP_ASSERT_LESS(a,b) \
upp11::TestCollection::getInstance().checkpoint(LOCATION, "UP_ASSERT_LESS"), \
upp11::TestAssert(LOCATION).assertTrue(a < b, #a " (" + std::to_string(a) + ") < " #b " (" + std::to_string(b) + ")")

//! Macro to test if a value is less than or equal to another
/*!
 * \param a First value to test
 * \param b Second value to test
 *
 * This assertion will pass if \a a <= \b b.
 */
#define UP_ASSERT_LESS_EQUAL(a,b) \
upp11::TestCollection::getInstance().checkpoint(LOCATION, "UP_ASSERT_LESS_EQUAL"), \
upp11::TestAssert(LOCATION).assertTrue(a <= b, #a " (" + std::to_string(a) + ") <= " #b " (" + std::to_string(b) + ")")

#endif // NEIGHBOR_TEST_UPP11_CONFIG_H_
