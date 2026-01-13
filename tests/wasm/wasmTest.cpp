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
#include "tudat/astro/basic_astro/astrodynamicsFunctions.h"
#include "tudat/astro/basic_astro/timeConversions.h"
#include "tudat/astro/basic_astro/stateVectorIndices.h"
#include "tudat/astro/basic_astro/modifiedEquinoctialElementConversions.h"
#include "tudat/astro/reference_frames/referenceFrameTransformations.h"
#include "tudat/math/basic/mathematicalConstants.h"
#include "tudat/math/basic/coordinateConversions.h"
#include "tudat/math/basic/legendrePolynomials.h"
#include "tudat/math/basic/linearAlgebra.h"
#include "tudat/math/basic/sphericalHarmonics.h"
#include "tudat/math/interpolators/linearInterpolator.h"
#include "tudat/math/interpolators/cubicSplineInterpolator.h"
#include "tudat/math/integrators/rungeKutta4Integrator.h"
#include "tudat/math/statistics/basicStatistics.h"

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
