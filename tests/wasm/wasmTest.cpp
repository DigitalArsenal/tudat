/*    Copyright (c) 2010-2024, Delft University of Technology
 *    All rights reserved
 *
 *    This file is part of the Tudat. Redistribution and use in source and
 *    binary forms, with or without modification, are permitted exclusively
 *    under the terms of the Modified BSD license. You should have received
 *    a copy of the license with this file. If not, please or visit:
 *    http://tudat.tudelft.nl/LICENSE.
 *
 *    Simple WASM test to validate the tudat library build.
 *    Run with: node wasmTest.js
 */

#include <iostream>
#include <cmath>
#include <iomanip>

#include <Eigen/Core>

#include "tudat/astro/basic_astro/orbitalElementConversions.h"
#include "tudat/astro/basic_astro/unitConversions.h"
#include "tudat/astro/basic_astro/physicalConstants.h"
#include "tudat/math/basic/mathematicalConstants.h"
#include "tudat/math/basic/coordinateConversions.h"

using namespace tudat;

// Simple test framework
int testsRun = 0;
int testsPassed = 0;
int testsFailed = 0;

void checkClose(const std::string& testName, double actual, double expected, double tolerance = 1e-10)
{
    testsRun++;
    double diff = std::abs(actual - expected);
    bool passed = diff < tolerance;

    if (passed) {
        testsPassed++;
        std::cout << "[PASS] " << testName << std::endl;
    } else {
        testsFailed++;
        std::cout << "[FAIL] " << testName << std::endl;
        std::cout << "       Expected: " << std::setprecision(15) << expected << std::endl;
        std::cout << "       Actual:   " << std::setprecision(15) << actual << std::endl;
        std::cout << "       Diff:     " << diff << " (tolerance: " << tolerance << ")" << std::endl;
    }
}

void checkVectorClose(const std::string& testName, const Eigen::Vector3d& actual,
                      const Eigen::Vector3d& expected, double tolerance = 1e-10)
{
    testsRun++;
    double diff = (actual - expected).norm();
    bool passed = diff < tolerance;

    if (passed) {
        testsPassed++;
        std::cout << "[PASS] " << testName << std::endl;
    } else {
        testsFailed++;
        std::cout << "[FAIL] " << testName << std::endl;
        std::cout << "       Expected: [" << expected.transpose() << "]" << std::endl;
        std::cout << "       Actual:   [" << actual.transpose() << "]" << std::endl;
        std::cout << "       Diff norm: " << diff << " (tolerance: " << tolerance << ")" << std::endl;
    }
}

void testUnitConversions()
{
    std::cout << "\n=== Unit Conversions ===" << std::endl;

    using namespace unit_conversions;

    // Test degree/radian conversions
    checkClose("180 degrees to radians",
               convertDegreesToRadians(180.0),
               mathematical_constants::PI);

    checkClose("PI radians to degrees",
               convertRadiansToDegrees(mathematical_constants::PI),
               180.0);

    // Test distance conversions
    checkClose("1 AU to meters",
               convertAstronomicalUnitsToMeters(1.0),
               physical_constants::ASTRONOMICAL_UNIT,
               1.0); // 1 meter tolerance

    checkClose("1 meter to AU",
               convertMetersToAstronomicalUnits(physical_constants::ASTRONOMICAL_UNIT),
               1.0,
               1e-15);
}

void testPhysicalConstants()
{
    std::cout << "\n=== Physical Constants ===" << std::endl;

    // Check some well-known constants
    checkClose("Speed of light",
               physical_constants::SPEED_OF_LIGHT,
               299792458.0,
               1.0);

    checkClose("Gravitational constant",
               physical_constants::GRAVITATIONAL_CONSTANT,
               6.67259e-11,  // Value from tudat
               1e-15);

    checkClose("Astronomical unit",
               physical_constants::ASTRONOMICAL_UNIT,
               1.495978707e11,
               1e3);  // 1 km tolerance
}

void testOrbitalElementConversions()
{
    std::cout << "\n=== Orbital Element Conversions ===" << std::endl;

    using namespace orbital_element_conversions;

    // Define a simple circular orbit (ISS-like)
    double semiMajorAxis = 6778.0e3;  // ~400 km altitude
    double eccentricity = 0.0001;      // Nearly circular
    double inclination = unit_conversions::convertDegreesToRadians(51.6);
    double argumentOfPeriapsis = 0.0;
    double longitudeOfAscendingNode = 0.0;
    double trueAnomaly = 0.0;

    // Earth gravitational parameter in m^3/s^2 (standard value)
    double earthGravParam = 3.986004418e14;

    // Create Keplerian elements vector
    Eigen::Vector6d keplerianElements;
    keplerianElements << semiMajorAxis, eccentricity, inclination,
                         argumentOfPeriapsis, longitudeOfAscendingNode, trueAnomaly;

    // Convert to Cartesian
    Eigen::Vector6d cartesianElements = convertKeplerianToCartesianElements(
        keplerianElements, earthGravParam);

    // Convert back to Keplerian
    Eigen::Vector6d keplerianRecovered = convertCartesianToKeplerianElements(
        cartesianElements, earthGravParam);

    // Check round-trip conversion
    checkClose("Semi-major axis round-trip",
               keplerianRecovered(0), semiMajorAxis, 1.0); // 1 meter tolerance

    checkClose("Eccentricity round-trip",
               keplerianRecovered(1), eccentricity, 1e-10);

    checkClose("Inclination round-trip",
               keplerianRecovered(2), inclination, 1e-10);

    // Check that position magnitude is approximately semi-major axis (for e≈0)
    Eigen::Vector3d position = cartesianElements.head<3>();
    checkClose("Position magnitude for circular orbit",
               position.norm(), semiMajorAxis, 1e3); // 1 km tolerance

    // Check orbital velocity (vis-viva for circular orbit: v = sqrt(mu/r))
    Eigen::Vector3d velocity = cartesianElements.tail<3>();
    double expectedVelocity = std::sqrt(earthGravParam / semiMajorAxis);
    checkClose("Velocity magnitude for circular orbit",
               velocity.norm(), expectedVelocity, 10.0); // 10 m/s tolerance
}

void testCoordinateConversions()
{
    std::cout << "\n=== Coordinate Conversions ===" << std::endl;

    using namespace coordinate_conversions;

    // Test Cartesian to Spherical and back
    // Point on x-axis at Earth radius
    Eigen::Vector3d cartesian(6378.0e3, 0.0, 0.0);

    // Spherical coordinates are (radius, zenith, azimuth)
    // For a point on x-axis: zenith = π/2, azimuth = 0
    Eigen::Vector3d spherical = convertCartesianToSpherical(cartesian);
    Eigen::Vector3d cartesianRecovered = convertSphericalToCartesian(spherical);

    checkVectorClose("Cartesian-Spherical round-trip", cartesianRecovered, cartesian, 1.0);

    // Check spherical coordinates (radius, zenith, azimuth)
    checkClose("Spherical radius", spherical(0), 6378.0e3, 1.0);
    checkClose("Spherical zenith (angle from z)", spherical(1), mathematical_constants::PI / 2.0, 1e-10);
    checkClose("Spherical azimuth (angle in xy)", spherical(2), 0.0, 1e-10);
}

void testEigenOperations()
{
    std::cout << "\n=== Eigen Matrix Operations ===" << std::endl;

    // Basic matrix operations to ensure Eigen works in WASM
    Eigen::Matrix3d rotation = Eigen::AngleAxisd(
        mathematical_constants::PI / 4, Eigen::Vector3d::UnitZ()).toRotationMatrix();

    Eigen::Vector3d v(1.0, 0.0, 0.0);
    Eigen::Vector3d rotated = rotation * v;

    // After 45-degree rotation around Z, x-unit vector should be at (sqrt(2)/2, sqrt(2)/2, 0)
    Eigen::Vector3d expected(std::sqrt(2.0)/2.0, std::sqrt(2.0)/2.0, 0.0);
    checkVectorClose("45-degree Z rotation", rotated, expected, 1e-14);

    // Check rotation matrix properties
    checkClose("Rotation matrix determinant", rotation.determinant(), 1.0, 1e-14);

    Eigen::Matrix3d shouldBeIdentity = rotation * rotation.transpose();
    checkClose("R * R^T = I (trace)", shouldBeIdentity.trace(), 3.0, 1e-14);
}

int main()
{
    std::cout << "========================================" << std::endl;
    std::cout << "  Tudat WASM Test Suite" << std::endl;
    std::cout << "========================================" << std::endl;

    try {
        testUnitConversions();
        testPhysicalConstants();
        testOrbitalElementConversions();
        testCoordinateConversions();
        testEigenOperations();
    } catch (const std::exception& e) {
        std::cerr << "\n[ERROR] Exception caught: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "\n========================================" << std::endl;
    std::cout << "  Test Results" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Tests run:    " << testsRun << std::endl;
    std::cout << "Tests passed: " << testsPassed << std::endl;
    std::cout << "Tests failed: " << testsFailed << std::endl;
    std::cout << "========================================" << std::endl;

    if (testsFailed > 0) {
        std::cout << "\n*** SOME TESTS FAILED ***" << std::endl;
        return 1;
    } else {
        std::cout << "\n*** ALL TESTS PASSED ***" << std::endl;
        return 0;
    }
}
