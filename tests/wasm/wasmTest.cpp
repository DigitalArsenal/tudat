/*    Copyright (c) 2010-2024, Delft University of Technology
 *    All rights reserved
 *
 *    This file is part of the Tudat. Redistribution and use in source and
 *    binary forms, with or without modification, are permitted exclusively
 *    under the terms of the Modified BSD license. You should have received
 *    a copy of the license with this file. If not, please or visit:
 *    http://tudat.tudelft.nl/LICENSE.
 *
 *    Comprehensive WASM test suite for Tudat library.
 *    Includes full propagation tests without requiring external SPICE kernels.
 *    Run with: node build-wasm/tests/wasm/tudat_wasm_test.js
 */

#include <iostream>
#include <cmath>
#include <iomanip>
#include <functional>
#include <memory>
#include <map>
#include <vector>

#include <Eigen/Core>

// Basic astrodynamics
#include "tudat/astro/basic_astro/orbitalElementConversions.h"
#include "tudat/astro/basic_astro/unitConversions.h"
#include "tudat/astro/basic_astro/physicalConstants.h"
#include "tudat/astro/basic_astro/astrodynamicsFunctions.h"
#include "tudat/astro/basic_astro/timeConversions.h"
#include "tudat/astro/basic_astro/stateVectorIndices.h"
#include "tudat/astro/basic_astro/modifiedEquinoctialElementConversions.h"
#include "tudat/astro/basic_astro/massRateModel.h"
#include "tudat/astro/reference_frames/referenceFrameTransformations.h"

// Mathematics
#include "tudat/math/basic/mathematicalConstants.h"
#include "tudat/math/basic/coordinateConversions.h"
#include "tudat/math/basic/legendrePolynomials.h"
#include "tudat/math/basic/linearAlgebra.h"
#include "tudat/math/basic/sphericalHarmonics.h"
#include "tudat/math/interpolators/linearInterpolator.h"
#include "tudat/math/interpolators/cubicSplineInterpolator.h"
#include "tudat/math/integrators/rungeKutta4Integrator.h"
#include "tudat/math/integrators/createNumericalIntegrator.h"
#include "tudat/math/statistics/basicStatistics.h"

// Propagation and simulation (for full propagation tests)
#include "tudat/simulation/propagation_setup/dynamicsSimulator.h"
#include "tudat/simulation/propagation_setup/propagationSettings.h"
#include "tudat/simulation/propagation_setup/propagationTerminationSettings.h"
#include "tudat/simulation/propagation_setup/accelerationSettings.h"
#include "tudat/simulation/propagation_setup/createAccelerationModels.h"
#include "tudat/simulation/propagation_setup/createMassRateModels.h"
#include "tudat/simulation/propagation_setup/propagationCR3BPFullProblem.h"
#include "tudat/simulation/environment_setup/body.h"
#include "tudat/simulation/environment_setup/createBodies.h"

// Ephemerides (for analytical orbits without SPICE)
#include "tudat/astro/ephemerides/constantEphemeris.h"

// Gravitation
#include "tudat/astro/gravitation/centralGravityModel.h"
#include "tudat/astro/gravitation/gravityFieldModel.h"

// Resource paths
#include "tudat/resource/resource.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

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

void checkTrue(const std::string& testName, bool condition)
{
    testsRun++;
    if (condition) {
        testsPassed++;
        std::cout << "[PASS] " << testName << std::endl;
    } else {
        testsFailed++;
        std::cout << "[FAIL] " << testName << std::endl;
    }
}

void checkStringEquals(const std::string& testName, const std::string& actual, const std::string& expected)
{
    testsRun++;
    if (actual == expected) {
        testsPassed++;
        std::cout << "[PASS] " << testName << std::endl;
    } else {
        testsFailed++;
        std::cout << "[FAIL] " << testName << std::endl;
        std::cout << "       Expected: \"" << expected << "\"" << std::endl;
        std::cout << "       Actual:   \"" << actual << "\"" << std::endl;
    }
}

void checkStringStartsWith(const std::string& testName, const std::string& actual, const std::string& prefix)
{
    testsRun++;
    bool passed = actual.substr(0, prefix.size()) == prefix;
    if (passed) {
        testsPassed++;
        std::cout << "[PASS] " << testName << std::endl;
    } else {
        testsFailed++;
        std::cout << "[FAIL] " << testName << std::endl;
        std::cout << "       Expected to start with: \"" << prefix << "\"" << std::endl;
        std::cout << "       Actual: \"" << actual << "\"" << std::endl;
    }
}

void checkVector6dClose(const std::string& testName, const Eigen::Vector6d& actual,
                        const Eigen::Vector6d& expected, double tolerance = 1e-10)
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

void testKeplerFunctions()
{
    std::cout << "\n=== Kepler Orbital Mechanics ===" << std::endl;

    using namespace basic_astrodynamics;

    // Test geostationary orbit period
    // Reference: http://en.wikipedia.org/wiki/Geostationary_orbit
    double satelliteMass = 1.0e3;  // kg
    double earthGravParam = physical_constants::GRAVITATIONAL_CONSTANT * 5.9736e24;
    double geoRadius = 4.2164e7;  // meters

    double orbitalPeriod = computeKeplerOrbitalPeriod(geoRadius, earthGravParam, satelliteMass);
    double expectedPeriod = 86164.09054;  // seconds (sidereal day)

    checkClose("Geostationary orbital period", orbitalPeriod, expectedPeriod, expectedPeriod * 1e-5);

    // Test mean motion
    double meanMotion = computeKeplerMeanMotion(geoRadius, earthGravParam, satelliteMass);
    double expectedMeanMotion = 2.0 * mathematical_constants::PI / expectedPeriod;

    checkClose("Geostationary mean motion", meanMotion, expectedMeanMotion, 1e-9);

    // Test synodic period (Earth-Mars example)
    double earthPeriod = 365.25 * 86400.0;  // seconds
    double marsPeriod = 687.0 * 86400.0;    // seconds
    double synodicPeriod = computeSynodicPeriod(earthPeriod, marsPeriod);
    double expectedSynodic = 779.94 * 86400.0;  // ~780 days

    checkClose("Earth-Mars synodic period", synodicPeriod, expectedSynodic, expectedSynodic * 0.01);

    // Test radial distance calculation
    double semiMajorAxis = 25999.683025291e3;
    double eccentricity = 0.864564003552322;
    double trueAnomaly = 0.757654217738482;
    double radialDistance = computeKeplerRadialDistance(semiMajorAxis, eccentricity, trueAnomaly);
    double expectedRadius = 4032815.56442827;

    checkClose("Kepler radial distance", radialDistance, expectedRadius, expectedRadius * 1e-5);
}

void testTimeConversions()
{
    std::cout << "\n=== Time Conversions ===" << std::endl;

    using namespace basic_astrodynamics;

    // Test Julian day conversions
    // J2000 epoch: January 1, 2000, 12:00 TT = JD 2451545.0
    double j2000JulianDay = 2451545.0;

    // Convert seconds since J2000 to Julian day
    double secondsSinceJ2000 = 0.0;
    double julianDay = convertSecondsSinceEpochToJulianDay(secondsSinceJ2000, j2000JulianDay);

    checkClose("J2000 epoch Julian day", julianDay, j2000JulianDay, 1e-10);

    // One day later
    double oneDaySeconds = 86400.0;
    julianDay = convertSecondsSinceEpochToJulianDay(oneDaySeconds, j2000JulianDay);

    checkClose("J2000 + 1 day Julian day", julianDay, j2000JulianDay + 1.0, 1e-10);

    // Convert back
    double recoveredSeconds = convertJulianDayToSecondsSinceEpoch(julianDay, j2000JulianDay);

    checkClose("Julian day round-trip", recoveredSeconds, oneDaySeconds, 1e-6);
}

void testLegendrePolynomials()
{
    std::cout << "\n=== Legendre Polynomials ===" << std::endl;

    using namespace basic_mathematics;

    // Test Legendre polynomials at known values
    // The tudat function computes associated Legendre polynomials P_l^m(x)
    // For m=0, these are the regular Legendre polynomials:
    // P_0^0(x) = 1
    // P_1^0(x) = x
    // P_2^0(x) = (3x^2 - 1)/2
    // P_3^0(x) = (5x^3 - 3x)/2

    double x = 0.5;
    int order = 0;  // m=0 gives regular Legendre polynomials

    double p0 = computeLegendrePolynomial(0, order, x);
    checkClose("P_0^0(0.5)", p0, 1.0, 1e-14);

    double p1 = computeLegendrePolynomial(1, order, x);
    checkClose("P_1^0(0.5)", p1, 0.5, 1e-14);

    double p2 = computeLegendrePolynomial(2, order, x);
    double expectedP2 = (3.0 * x * x - 1.0) / 2.0;
    checkClose("P_2^0(0.5)", p2, expectedP2, 1e-14);

    double p3 = computeLegendrePolynomial(3, order, x);
    double expectedP3 = (5.0 * x * x * x - 3.0 * x) / 2.0;
    checkClose("P_3^0(0.5)", p3, expectedP3, 1e-14);
}

void testLinearInterpolation()
{
    std::cout << "\n=== Linear Interpolation ===" << std::endl;

    using namespace interpolators;

    // Create simple data: y = 2x + 1
    std::map<double, double> dataMap;
    dataMap[0.0] = 1.0;
    dataMap[1.0] = 3.0;
    dataMap[2.0] = 5.0;
    dataMap[3.0] = 7.0;

    LinearInterpolator<double, double> interpolator(dataMap);

    // Test at known points
    checkClose("Interpolate at x=0", interpolator.interpolate(0.0), 1.0, 1e-14);
    checkClose("Interpolate at x=1", interpolator.interpolate(1.0), 3.0, 1e-14);
    checkClose("Interpolate at x=2", interpolator.interpolate(2.0), 5.0, 1e-14);

    // Test at intermediate points
    checkClose("Interpolate at x=0.5", interpolator.interpolate(0.5), 2.0, 1e-14);
    checkClose("Interpolate at x=1.5", interpolator.interpolate(1.5), 4.0, 1e-14);
    checkClose("Interpolate at x=2.5", interpolator.interpolate(2.5), 6.0, 1e-14);
}

void testNumericalIntegration()
{
    std::cout << "\n=== Numerical Integration ===" << std::endl;

    using namespace numerical_integrators;

    // Test RK4 on simple ODE: dy/dt = y, y(0) = 1
    // Exact solution: y = e^t

    // State derivative function
    auto stateDerivative = [](const double t, const Eigen::VectorXd& state) -> Eigen::VectorXd {
        return state;  // dy/dt = y
    };

    // Initial conditions
    double t0 = 0.0;
    Eigen::VectorXd y0(1);
    y0 << 1.0;

    // Create RK4 integrator
    double stepSize = 0.01;
    RungeKutta4Integrator<double, Eigen::VectorXd> integrator(stateDerivative, t0, y0, stepSize);

    // Integrate to t = 1
    double tEnd = 1.0;
    while (integrator.getCurrentIndependentVariable() < tEnd) {
        integrator.performIntegrationStep(stepSize);
    }

    double computedY = integrator.getCurrentState()(0);
    double exactY = std::exp(1.0);

    checkClose("RK4 exponential growth", computedY, exactY, 1e-6);

    // Test on harmonic oscillator: d²x/dt² = -x
    // Rewrite as system: dx/dt = v, dv/dt = -x
    // Initial: x(0) = 1, v(0) = 0
    // Exact: x(t) = cos(t)

    auto harmonicDerivative = [](const double t, const Eigen::VectorXd& state) -> Eigen::VectorXd {
        Eigen::VectorXd derivative(2);
        derivative(0) = state(1);   // dx/dt = v
        derivative(1) = -state(0);  // dv/dt = -x
        return derivative;
    };

    Eigen::VectorXd harmonicState(2);
    harmonicState << 1.0, 0.0;  // x=1, v=0

    RungeKutta4Integrator<double, Eigen::VectorXd> harmonicIntegrator(
        harmonicDerivative, 0.0, harmonicState, stepSize);

    // Integrate to t = π/2
    double tHalf = mathematical_constants::PI / 2.0;
    while (harmonicIntegrator.getCurrentIndependentVariable() < tHalf - stepSize/2) {
        harmonicIntegrator.performIntegrationStep(stepSize);
    }

    double computedX = harmonicIntegrator.getCurrentState()(0);
    double exactX = std::cos(tHalf);  // Should be ~0

    // RK4 accumulates some error over many steps, use absolute tolerance
    checkClose("RK4 harmonic oscillator x(pi/2)", computedX, exactX, 1e-3);
}

void testCubicSplineInterpolation()
{
    std::cout << "\n=== Cubic Spline Interpolation ===" << std::endl;

    using namespace interpolators;

    // Create data from sin function
    std::map<double, double> dataMap;
    for (int i = 0; i <= 10; i++) {
        double x = i * mathematical_constants::PI / 10.0;
        dataMap[x] = std::sin(x);
    }

    CubicSplineInterpolator<double, double> spline(dataMap);

    // Test at intermediate points
    double x1 = mathematical_constants::PI / 4.0;
    double y1 = spline.interpolate(x1);
    checkClose("Cubic spline sin(pi/4)", y1, std::sin(x1), 1e-4);

    double x2 = mathematical_constants::PI / 3.0;
    double y2 = spline.interpolate(x2);
    checkClose("Cubic spline sin(pi/3)", y2, std::sin(x2), 1e-4);

    double x3 = mathematical_constants::PI / 2.0;
    double y3 = spline.interpolate(x3);
    checkClose("Cubic spline sin(pi/2)", y3, 1.0, 1e-10);
}

void testReferenceFrameTransformations()
{
    std::cout << "\n=== Reference Frame Transformations ===" << std::endl;

    using namespace reference_frames;

    // Test getRotatingPlanetocentricToInertialFrameTransformationMatrix
    double angle = mathematical_constants::PI / 6.0;  // 30 degrees
    Eigen::Matrix3d toInertial = getRotatingPlanetocentricToInertialFrameTransformationMatrix(angle);
    Eigen::Matrix3d toRotating = getInertialToPlanetocentricFrameTransformationMatrix(angle);

    // The rotation should be well-formed (orthogonal)
    Eigen::Matrix3d shouldBeIdentity = toInertial * toInertial.transpose();
    checkClose("Rotation matrix is orthogonal (trace)", shouldBeIdentity.trace(), 3.0, 1e-14);

    // These should be inverses of each other
    Eigen::Matrix3d product = toInertial * toRotating;
    checkClose("Planetocentric transforms are inverses (trace)", product.trace(), 3.0, 1e-14);

    // Test a specific transformation: rotate a vector on x-axis
    Eigen::Vector3d xAxis(1.0, 0.0, 0.0);
    Eigen::Vector3d rotated = toInertial * xAxis;

    // After rotation by 30 degrees around Z, x should go to (cos(30), sin(30), 0)
    double expectedX = std::cos(angle);
    double expectedY = std::sin(angle);
    checkClose("Planetocentric X rotation x-comp", rotated(0), expectedX, 1e-14);
    checkClose("Planetocentric X rotation y-comp", rotated(1), expectedY, 1e-14);
    checkClose("Planetocentric X rotation z-comp", rotated(2), 0.0, 1e-14);
}

void testModifiedEquinoctialElements()
{
    std::cout << "\n=== Modified Equinoctial Elements ===" << std::endl;

    using namespace orbital_element_conversions;

    // Define a test orbit (Mars-like)
    double earthGravParam = 3.986004418e14;
    double semiMajorAxis = 10000.0e3;  // 10,000 km
    double eccentricity = 0.1;
    double inclination = unit_conversions::convertDegreesToRadians(30.0);
    double argumentOfPeriapsis = unit_conversions::convertDegreesToRadians(45.0);
    double raan = unit_conversions::convertDegreesToRadians(60.0);
    double trueAnomaly = unit_conversions::convertDegreesToRadians(90.0);

    // Create Keplerian elements vector
    Eigen::Vector6d keplerianElements;
    keplerianElements << semiMajorAxis, eccentricity, inclination,
                         argumentOfPeriapsis, raan, trueAnomaly;

    // Convert to Modified Equinoctial Elements
    Eigen::Vector6d mee = convertKeplerianToModifiedEquinoctialElements(
        keplerianElements, true);  // true = use semi-latus rectum representation

    // Convert back to Keplerian
    Eigen::Vector6d keplerianRecovered = convertModifiedEquinoctialToKeplerianElements(
        mee, true);

    // Check round-trip conversion
    checkClose("MEE semi-major axis round-trip",
               keplerianRecovered(0), semiMajorAxis, 1.0);
    checkClose("MEE eccentricity round-trip",
               keplerianRecovered(1), eccentricity, 1e-10);
    checkClose("MEE inclination round-trip",
               keplerianRecovered(2), inclination, 1e-10);
    checkClose("MEE arg periapsis round-trip",
               keplerianRecovered(3), argumentOfPeriapsis, 1e-10);
    checkClose("MEE RAAN round-trip",
               keplerianRecovered(4), raan, 1e-10);
    checkClose("MEE true anomaly round-trip",
               keplerianRecovered(5), trueAnomaly, 1e-10);
}

void testStatistics()
{
    std::cout << "\n=== Statistics ===" << std::endl;

    using namespace statistics;

    // Test sample mean
    std::vector<double> data = {1.0, 2.0, 3.0, 4.0, 5.0};
    double mean = computeSampleMean(data);
    checkClose("Sample mean of 1-5", mean, 3.0, 1e-14);

    // Test with another data set
    std::vector<double> data2 = {2.0, 4.0, 6.0, 8.0, 10.0};
    double mean2 = computeSampleMean(data2);
    checkClose("Sample mean of 2,4,6,8,10", mean2, 6.0, 1e-14);

    // Test sample variance
    double variance = computeSampleVariance(data);
    // Variance of 1,2,3,4,5: mean=3, sum of squared deviations = 4+1+0+1+4 = 10
    // Sample variance = 10/4 = 2.5
    checkClose("Sample variance of 1-5", variance, 2.5, 1e-14);
}

void testSphericalHarmonics()
{
    std::cout << "\n=== Spherical Harmonics ===" << std::endl;

    using namespace basic_mathematics;

    // Test spherical harmonic computations
    // For degree 0, order 0: Y_0^0 = 1/sqrt(4*pi)
    double cosineLatitude = 0.5;  // latitude = 60 degrees

    // Compute geodesy-normalized Legendre polynomial
    double p00 = computeGeodesyLegendrePolynomial(0, 0, cosineLatitude);
    checkClose("Geodesy P_0^0(0.5)", p00, 1.0, 1e-14);

    double p10 = computeGeodesyLegendrePolynomial(1, 0, cosineLatitude);
    // For geodesy normalization, P_1^0(x) = sqrt(3) * x
    double expectedP10 = std::sqrt(3.0) * cosineLatitude;
    checkClose("Geodesy P_1^0(0.5)", p10, expectedP10, 1e-14);

    double p11 = computeGeodesyLegendrePolynomial(1, 1, cosineLatitude);
    // P_1^1(x) = sqrt(3) * sqrt(1-x^2)
    double sinLatitude = std::sqrt(1.0 - cosineLatitude * cosineLatitude);
    double expectedP11 = std::sqrt(3.0) * sinLatitude;
    checkClose("Geodesy P_1^1(0.5)", p11, expectedP11, 1e-14);
}

void testResourcePaths()
{
    std::cout << "\n=== Resource Paths (WASM Virtual FS) ===" << std::endl;

    using namespace paths;

    // Test that all resource paths are properly defined and point to the WASM data mount point
    std::string basePath = "/tudat_data";

    checkStringEquals("Base resources path", get_resources_path(), basePath);
    checkStringEquals("Ephemeris path", get_ephemeris_path(), basePath + "/ephemeris");
    checkStringEquals("Earth orientation path", get_earth_orientation_path(), basePath + "/earth_orientation");
    checkStringEquals("Quadrature path", get_quadrature_path(), basePath + "/quadrature");
    checkStringEquals("SPICE kernels path", get_spice_kernels_path(), basePath + "/spice_kernels");
    checkStringEquals("Atmosphere tables path", get_atmosphere_tables_path(), basePath + "/atmosphere_tables");
    checkStringEquals("Gravity models path", get_gravity_models_path(), basePath + "/gravity_models");
    checkStringEquals("Space weather path", get_space_weather_path(), basePath + "/space_weather");

    // Verify all paths start with the base path (consistency check)
    checkStringStartsWith("Ephemeris path prefix", get_ephemeris_path(), basePath);
    checkStringStartsWith("SPICE kernels path prefix", get_spice_kernels_path(), basePath);
}

#ifdef __EMSCRIPTEN__
void testEmscriptenEnvironment()
{
    std::cout << "\n=== Emscripten Environment ===" << std::endl;

    // Test that we're running in the Emscripten environment
    checkTrue("Running in Emscripten", true);

    // Test that the virtual filesystem is available (basic check)
    // In a full WASM environment, you could test FS operations here
    checkTrue("WASM environment detected", emscripten_run_script_int("1") == 1);
}
#endif

void testLinearAlgebra()
{
    std::cout << "\n=== Linear Algebra Operations ===" << std::endl;

    using namespace linear_algebra;

    // Test cross product
    Eigen::Vector3d a(1.0, 0.0, 0.0);
    Eigen::Vector3d b(0.0, 1.0, 0.0);
    Eigen::Vector3d cross = a.cross(b);
    Eigen::Vector3d expectedCross(0.0, 0.0, 1.0);
    checkVectorClose("Cross product x × y = z", cross, expectedCross, 1e-14);

    // Test getCrossProductMatrix
    Eigen::Matrix3d crossMatrix = getCrossProductMatrix(a);
    Eigen::Vector3d crossFromMatrix = crossMatrix * b;
    checkVectorClose("Cross product matrix", crossFromMatrix, expectedCross, 1e-14);

    // Verify cross product matrix property: [a×] * b = a × b
    Eigen::Vector3d c(1.0, 2.0, 3.0);
    Eigen::Vector3d d(4.0, 5.0, 6.0);
    Eigen::Matrix3d cCrossMatrix = getCrossProductMatrix(c);
    Eigen::Vector3d crossDirect = c.cross(d);
    Eigen::Vector3d crossViaMatrix = cCrossMatrix * d;
    checkVectorClose("Cross product matrix general case", crossViaMatrix, crossDirect, 1e-14);
}

// ============================================================================
// PROPAGATION TESTS - Full dynamics simulation without external SPICE kernels
// ============================================================================

void testCR3BPPropagation()
{
    std::cout << "\n=== CR3BP Propagation (Circular Restricted 3-Body Problem) ===" << std::endl;

    using namespace propagators;
    using namespace numerical_integrators;
    using namespace orbital_element_conversions;

    // Test CR3BP propagation - this is a self-contained dynamical system
    // that doesn't require any external data files or SPICE kernels.
    // Reference values independently generated by Y. Liu (from Tudat unit tests)

    // Define initial normalized state (Sun-Earth-Moon like system)
    Eigen::Vector6d initialState;
    initialState[xCartesianPositionIndex] = 0.994;
    initialState[yCartesianPositionIndex] = 0.853;
    initialState[zCartesianPositionIndex] = 0.312;
    initialState[xCartesianVelocityIndex] = 0.195;
    initialState[yCartesianVelocityIndex] = -0.211;
    initialState[zCartesianVelocityIndex] = 0.15;

    // Set normalized mass parameter and propagation settings
    double massParameter = 2.528e-5;  // Normalized mass ratio
    double simulationStartEpoch = 0.0;
    double simulationEndEpoch = 20.0;  // Normalized time
    double timeStep = 0.0001;

    // Create integrator settings (RK4)
    std::shared_ptr<IntegratorSettings<>> integratorSettings =
        std::make_shared<IntegratorSettings<>>(rungeKutta4, simulationStartEpoch, timeStep);

    // Propagate CR3BP dynamics
    std::map<double, Eigen::Vector6d> stateHistory = performCR3BPIntegration(
        integratorSettings, massParameter, initialState,
        simulationStartEpoch, simulationEndEpoch);

    // Get final state
    auto stateIterator = stateHistory.rbegin();
    if (stateIterator->first - simulationEndEpoch > timeStep / 2.0) {
        stateIterator++;
    }

    // Expected final state (independently computed reference)
    Eigen::Vector6d expectedFinalState;
    expectedFinalState << -1.34313636385140, -1.54200249942130, -0.416194453794142,
                          -0.863033291171519, 1.12530842202949, 0.181821699265344;

    // Check each component
    for (int i = 0; i < 6; i++) {
        std::string componentName = "CR3BP final state component " + std::to_string(i);
        checkClose(componentName, stateIterator->second(i), expectedFinalState(i), 1e-10);
    }

    // Verify propagation produced reasonable number of steps
    checkTrue("CR3BP propagation steps > 100", stateHistory.size() > 100);
}

void testCustomStatePropagation()
{
    std::cout << "\n=== Custom State Propagation ===" << std::endl;

    using namespace propagators;
    using namespace simulation_setup;
    using namespace numerical_integrators;

    // Test custom state propagation - demonstrates the ODE framework works in WASM
    // This propagates a simple linear ODE: dS/dt = -0.02 (constant rate)
    // Exact solution: S(t) = S0 - 0.02*t

    // Create empty body container (custom state doesn't need bodies)
    SystemOfBodies bodies;

    // Define state derivative function
    auto stateDerivativeFunction = [](const double currentTime, const double currentState) -> double {
        return -0.02;  // Constant decay rate
    };

    // Initial state
    double initialState = 500.0;

    // Create propagator settings
    std::shared_ptr<CustomStatePropagatorSettings<double>> propagatorSettings =
        std::make_shared<CustomStatePropagatorSettings<double>>(
            std::bind(stateDerivativeFunction, std::placeholders::_1, std::placeholders::_2),
            initialState,
            std::make_shared<PropagationTimeTerminationSettings>(1000.0));

    // Create integrator settings
    std::shared_ptr<IntegratorSettings<>> integratorSettings =
        std::make_shared<IntegratorSettings<>>(rungeKutta4, 0.0, 1.0);

    // Run the dynamics simulation
    SingleArcDynamicsSimulator<double, double> dynamicsSimulator(
        bodies, integratorSettings, propagatorSettings, true, false, false);

    // Get propagated solution
    std::map<double, Eigen::VectorXd> integratedState =
        dynamicsSimulator.getEquationsOfMotionNumericalSolution();

    // Verify a few points in the solution
    int checksPerformed = 0;
    for (const auto& stateEntry : integratedState) {
        double time = stateEntry.first;
        double state = stateEntry.second(0);
        double expectedState = 500.0 - 0.02 * time;

        // Check that state matches analytical solution
        if (std::abs(state - expectedState) >= 1e-9) {
            checkClose("Custom state at t=" + std::to_string(time), state, expectedState, 1e-9);
        }
        checksPerformed++;

        // Only check a subset of points to keep output manageable
        if (checksPerformed >= 5) break;
    }

    // Final state check
    auto finalState = integratedState.rbegin();
    double expectedFinal = 500.0 - 0.02 * finalState->first;
    checkClose("Custom state final value", finalState->second(0), expectedFinal, 1e-9);

    checkTrue("Custom propagation completed", integratedState.size() > 0);
}

void testMassPropagation()
{
    std::cout << "\n=== Mass Propagation ===" << std::endl;

    using namespace propagators;
    using namespace simulation_setup;
    using namespace numerical_integrators;

    // Test mass propagation - simulates fuel consumption
    // Mass rate: dm/dt = -0.01 kg/s (constant burn)
    // Exact solution: m(t) = m0 - 0.01*t

    // Create body container with a vehicle
    SystemOfBodies bodies;
    bodies.createEmptyBody("Vehicle");

    // Create mass rate model (constant fuel burn)
    std::map<std::string, std::shared_ptr<basic_astrodynamics::MassRateModel>> massRateModels;
    massRateModels["Vehicle"] = std::make_shared<basic_astrodynamics::CustomMassRateModel>(
        [](const double) { return -0.01; });

    // Initial mass
    Eigen::VectorXd initialMass = Eigen::VectorXd(1);
    initialMass(0) = 500.0;  // 500 kg initial mass

    // Create propagator settings
    std::shared_ptr<PropagatorSettings<double>> propagatorSettings =
        std::make_shared<MassPropagatorSettings<double>>(
            std::vector<std::string>{"Vehicle"},
            massRateModels,
            initialMass,
            std::make_shared<PropagationTimeTerminationSettings>(1000.0));

    // Create integrator settings
    std::shared_ptr<IntegratorSettings<>> integratorSettings =
        std::make_shared<IntegratorSettings<>>(rungeKutta4, 0.0, 1.0);

    // Run dynamics simulation
    SingleArcDynamicsSimulator<double, double> dynamicsSimulator(
        bodies, integratorSettings, propagatorSettings, true, false, false);

    // Get results
    std::map<double, Eigen::VectorXd> massHistory =
        dynamicsSimulator.getEquationsOfMotionNumericalSolution();

    // Check initial mass
    checkClose("Mass at t=0", massHistory.begin()->second(0), 500.0, 1e-13);

    // Check final mass (at t=1000, mass should be 500 - 0.01*1000 = 490 kg)
    auto finalEntry = massHistory.rbegin();
    double expectedFinalMass = 500.0 - 0.01 * finalEntry->first;
    checkClose("Mass at final time", finalEntry->second(0), expectedFinalMass, 1e-10);

    checkTrue("Mass propagation completed", massHistory.size() > 0);
}

void testTwoBodyPropagation()
{
    std::cout << "\n=== Two-Body Orbit Propagation ===" << std::endl;

    using namespace propagators;
    using namespace simulation_setup;
    using namespace numerical_integrators;
    using namespace orbital_element_conversions;
    using namespace gravitation;

    // Test two-body orbit propagation using analytical ephemerides (no SPICE needed)
    // This creates a simple Earth-spacecraft system and propagates for one orbit

    // Create bodies
    SystemOfBodies bodies;
    bodies.createEmptyBody("Earth");
    bodies.createEmptyBody("Spacecraft");

    // Set Earth properties - constant ephemeris at origin
    bodies.at("Earth")->setEphemeris(
        std::make_shared<ephemerides::ConstantEphemeris>(
            []() { return Eigen::Vector6d::Zero(); },
            "SSB", "J2000"));

    // Set Earth gravity field (central body)
    double earthGravParam = 3.986004418e14;  // m^3/s^2
    bodies.at("Earth")->setGravityFieldModel(
        std::make_shared<GravityFieldModel>(earthGravParam));

    // Define spacecraft initial state (circular LEO orbit)
    double semiMajorAxis = 7000.0e3;  // 7000 km (~630 km altitude)
    double orbitalPeriod = 2.0 * mathematical_constants::PI *
                           std::sqrt(std::pow(semiMajorAxis, 3) / earthGravParam);

    Eigen::Vector6d keplerianElements;
    keplerianElements << semiMajorAxis,  // a
                         0.0,            // e (circular)
                         unit_conversions::convertDegreesToRadians(45.0),  // i
                         0.0,            // omega
                         0.0,            // RAAN
                         0.0;            // true anomaly

    Eigen::Vector6d initialCartesianState = convertKeplerianToCartesianElements(
        keplerianElements, earthGravParam);

    // Set spacecraft ephemeris (will be overwritten by propagation)
    bodies.at("Spacecraft")->setEphemeris(
        std::make_shared<ephemerides::ConstantEphemeris>(
            [=]() { return initialCartesianState; },
            "Earth", "J2000"));

    // Define acceleration map (point-mass gravity only)
    SelectedAccelerationMap accelerationMap;
    accelerationMap["Spacecraft"]["Earth"].push_back(
        std::make_shared<AccelerationSettings>(basic_astrodynamics::point_mass_gravity));

    std::vector<std::string> bodiesToPropagate = {"Spacecraft"};
    std::vector<std::string> centralBodies = {"Earth"};

    // Create acceleration models
    basic_astrodynamics::AccelerationMap accelerationModelMap = createAccelerationModelsMap(
        bodies, accelerationMap, bodiesToPropagate, centralBodies);

    // Create propagator settings
    std::shared_ptr<TranslationalStatePropagatorSettings<double>> propagatorSettings =
        std::make_shared<TranslationalStatePropagatorSettings<double>>(
            centralBodies,
            accelerationModelMap,
            bodiesToPropagate,
            initialCartesianState,
            0.0,
            std::make_shared<IntegratorSettings<>>(rungeKutta4, 0.0, 10.0),
            std::make_shared<PropagationTimeTerminationSettings>(orbitalPeriod));

    // Run dynamics simulation
    SingleArcDynamicsSimulator<double, double> dynamicsSimulator(bodies, propagatorSettings);

    // Get results
    std::map<double, Eigen::VectorXd> stateHistory =
        dynamicsSimulator.getEquationsOfMotionNumericalSolution();

    // Verify propagation ran
    checkTrue("Two-body propagation completed", stateHistory.size() > 0);

    // For a circular orbit, after one period the spacecraft should return to initial position
    // (within numerical tolerance)
    auto finalState = stateHistory.rbegin();
    Eigen::Vector3d initialPosition = initialCartesianState.head<3>();
    Eigen::Vector3d finalPosition = finalState->second.head<3>();

    double positionError = (finalPosition - initialPosition).norm();
    // Allow 1 km error for full orbit propagation (numerical integration error)
    checkTrue("Return to initial position (within 1 km)", positionError < 1000.0);

    // Verify orbital energy is conserved (within tolerance)
    double initialEnergy = 0.5 * initialCartesianState.tail<3>().squaredNorm() -
                          earthGravParam / initialPosition.norm();
    double finalEnergy = 0.5 * finalState->second.tail<3>().squaredNorm() -
                        earthGravParam / finalPosition.norm();
    double energyError = std::abs(finalEnergy - initialEnergy) / std::abs(initialEnergy);
    checkTrue("Orbital energy conserved (< 0.01%)", energyError < 1e-4);
}

void testMultiBodyMassPropagation()
{
    std::cout << "\n=== Multi-Body Coupled Mass Propagation ===" << std::endl;

    using namespace propagators;
    using namespace simulation_setup;
    using namespace numerical_integrators;

    // Test coupled mass propagation with two vehicles
    // This tests the multi-body propagation framework

    // Create bodies
    SystemOfBodies bodies;
    bodies.createEmptyBody("Earth");
    bodies.createEmptyBody("Vehicle1");
    bodies.createEmptyBody("Vehicle2");

    // Set ephemerides (constant, at origin for simplicity)
    bodies.at("Earth")->setEphemeris(
        std::make_shared<ephemerides::ConstantEphemeris>(
            []() { return Eigen::Vector6d::Zero(); }));
    bodies.at("Vehicle1")->setEphemeris(
        std::make_shared<ephemerides::ConstantEphemeris>(
            []() { return Eigen::Vector6d::Zero(); }, "Earth"));
    bodies.at("Vehicle2")->setEphemeris(
        std::make_shared<ephemerides::ConstantEphemeris>(
            []() { return Eigen::Vector6d::Zero(); }, "Earth"));

    // Define mass rate functions that depend on both masses
    // dm1/dt = -(m1 + 2*m2) / 1e4
    // dm2/dt = -(3*m1 + 2*m2) / 1e4
    auto getMassRate1 = [&bodies]() {
        return -(bodies.at("Vehicle1")->getBodyMass() +
                 2.0 * bodies.at("Vehicle2")->getBodyMass()) / 1.0e4;
    };
    auto getMassRate2 = [&bodies]() {
        return -(3.0 * bodies.at("Vehicle1")->getBodyMass() +
                 2.0 * bodies.at("Vehicle2")->getBodyMass()) / 1.0e4;
    };

    // Create mass rate models
    std::map<std::string, std::vector<std::shared_ptr<basic_astrodynamics::MassRateModel>>> massRateModels;
    massRateModels["Vehicle1"].push_back(
        std::make_shared<basic_astrodynamics::CustomMassRateModel>(
            [getMassRate1](const double) { return getMassRate1(); }));
    massRateModels["Vehicle2"].push_back(
        std::make_shared<basic_astrodynamics::CustomMassRateModel>(
            [getMassRate2](const double) { return getMassRate2(); }));

    // Initial masses
    Eigen::VectorXd initialMasses(2);
    initialMasses(0) = 500.0;   // Vehicle1: 500 kg
    initialMasses(1) = 1000.0;  // Vehicle2: 1000 kg

    // Create integrator settings
    std::shared_ptr<IntegratorSettings<>> integratorSettings = rungeKutta4Settings(1.0);

    // Create propagator settings
    std::shared_ptr<SingleArcPropagatorSettings<double>> propagatorSettings =
        std::make_shared<MassPropagatorSettings<double>>(
            std::vector<std::string>{"Vehicle1", "Vehicle2"},
            massRateModels,
            initialMasses,
            0.0,
            integratorSettings,
            std::make_shared<PropagationTimeTerminationSettings>(1000.0));
    propagatorSettings->getOutputSettingsBase()->setIntegratedResult(true);

    // Run simulation
    SingleArcDynamicsSimulator<double, double> dynamicsSimulator(bodies, propagatorSettings);

    // Get results
    std::map<double, Eigen::VectorXd> massHistory =
        dynamicsSimulator.getEquationsOfMotionNumericalSolution();

    // Verify initial state
    checkClose("Vehicle1 initial mass", massHistory.begin()->second(0), 500.0, 1e-10);
    checkClose("Vehicle2 initial mass", massHistory.begin()->second(1), 1000.0, 1e-10);

    // The analytical solution for this coupled system is:
    // m1(t) = 100 * (-exp(-t/1e4) + 6*exp(4t/1e4))
    // m2(t) = 100 * (exp(-t/1e4) + 9*exp(4t/1e4))
    // Check at final time
    auto finalEntry = massHistory.rbegin();
    double t = finalEntry->first;
    double expectedM1 = 100.0 * (-std::exp(-t / 1e4) + 6.0 * std::exp(4.0 * t / 1e4));
    double expectedM2 = 100.0 * (std::exp(-t / 1e4) + 9.0 * std::exp(4.0 * t / 1e4));

    checkClose("Vehicle1 final mass (coupled)", finalEntry->second(0), expectedM1, 1e-10);
    checkClose("Vehicle2 final mass (coupled)", finalEntry->second(1), expectedM2, 1e-10);

    checkTrue("Multi-body mass propagation completed", massHistory.size() > 0);
}

void testPropagationTermination()
{
    std::cout << "\n=== Propagation Termination Conditions ===" << std::endl;

    using namespace propagators;
    using namespace simulation_setup;
    using namespace numerical_integrators;

    // Test that propagation termination works correctly
    // We'll propagate a custom state and verify it stops at the right time

    SystemOfBodies bodies;

    // Exponential decay: dS/dt = -0.002 * S
    // Solution: S(t) = S0 * exp(-0.002*t)
    auto expDecayDerivative = [](const double t, const double state) -> double {
        return -0.002 * state;
    };

    double initialState = 500.0;
    double terminationTime = 500.0;

    std::shared_ptr<CustomStatePropagatorSettings<double>> propagatorSettings =
        std::make_shared<CustomStatePropagatorSettings<double>>(
            std::bind(expDecayDerivative, std::placeholders::_1, std::placeholders::_2),
            initialState,
            std::make_shared<PropagationTimeTerminationSettings>(terminationTime));

    std::shared_ptr<IntegratorSettings<>> integratorSettings =
        std::make_shared<IntegratorSettings<>>(rungeKutta4, 0.0, 1.0);

    SingleArcDynamicsSimulator<double, double> dynamicsSimulator(
        bodies, integratorSettings, propagatorSettings, true, false, false);

    std::map<double, Eigen::VectorXd> stateHistory =
        dynamicsSimulator.getEquationsOfMotionNumericalSolution();

    // Verify propagation stopped at the correct time
    auto finalEntry = stateHistory.rbegin();
    checkClose("Termination time reached", finalEntry->first, terminationTime, 1.0);

    // Verify final state matches analytical solution
    double expectedFinalState = initialState * std::exp(-0.002 * finalEntry->first);
    checkClose("Exponential decay final state", finalEntry->second(0), expectedFinalState, 1e-6);
}

int main()
{
    std::cout << "========================================" << std::endl;
    std::cout << "  Tudat WASM Test Suite" << std::endl;
    std::cout << "========================================" << std::endl;

    try {
        // Basic astrodynamics and math tests
        testUnitConversions();
        testPhysicalConstants();
        testOrbitalElementConversions();
        testCoordinateConversions();
        testEigenOperations();
        testKeplerFunctions();
        testTimeConversions();
        testLegendrePolynomials();
        testLinearInterpolation();
        testNumericalIntegration();
        testCubicSplineInterpolation();
        testReferenceFrameTransformations();
        testModifiedEquinoctialElements();
        testStatistics();
        testSphericalHarmonics();
        testLinearAlgebra();
        testResourcePaths();

        // Propagation tests (full dynamics simulation)
        std::cout << "\n========================================" << std::endl;
        std::cout << "  PROPAGATION TESTS" << std::endl;
        std::cout << "========================================" << std::endl;

        testCR3BPPropagation();           // Circular Restricted 3-Body Problem
        testCustomStatePropagation();     // Custom ODE propagation
        testMassPropagation();            // Single body mass propagation
        testTwoBodyPropagation();         // Two-body orbit propagation
        testMultiBodyMassPropagation();   // Coupled multi-body mass propagation
        testPropagationTermination();     // Termination conditions

#ifdef __EMSCRIPTEN__
        testEmscriptenEnvironment();
#endif
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
