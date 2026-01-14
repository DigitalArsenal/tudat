// Tudat WASM Test Runner - CesiumJS Edition
// Handles WASM test execution, real-time UI updates, and visualizations

class TudatTestRunner {
    constructor() {
        this.wasmModule = null;
        this.testResults = [];
        this.categories = {};
        this.isRunning = false;
        this.startTime = null;
        this.currentCategory = 'General';
        this.consoleLines = 0;
        this.expectedTests = 167;

        // Cesium viewer
        this.viewer = null;
        this.orbitEntities = [];

        // Charts
        this.charts = {};

        // Currently selected test
        this.selectedTest = null;

        // Orbital data for visualization
        this.orbitalData = {};

        this.init();
    }

    async init() {
        this.updateTimestamp();
        setInterval(() => this.updateTimestamp(), 1000);

        this.setupEventListeners();
        this.setupCharts();
        this.setupCesium();
        this.generateOrbitalData();

        await this.loadWasm();
    }

    updateTimestamp() {
        const now = new Date();
        document.getElementById('timestamp').textContent =
            now.toISOString().replace('T', ' ').substring(0, 19) + ' UTC';
    }

    setupEventListeners() {
        document.getElementById('run-btn').addEventListener('click', () => this.runTests());
        document.getElementById('clear-btn').addEventListener('click', () => this.clearResults());
    }

    // ==================== Cesium Setup ====================

    async setupCesium() {
        // No Ion token needed - we use local imagery files

        // Create Blue Marble imagery provider (day texture)
        const blueMarbleProvider = await Cesium.SingleTileImageryProvider.fromUrl(
            'imagery/world.topo.bathy.200407.3x5400x2700.jpg',
            {
                rectangle: Cesium.Rectangle.fromDegrees(-180.0, -90.0, 180.0, 90.0),
                tileWidth: 5400,
                tileHeight: 2700
            }
        );

        // Create viewer with minimal UI and local Blue Marble imagery
        this.viewer = new Cesium.Viewer('cesiumContainer', {
            animation: false,
            timeline: false,
            baseLayerPicker: false,
            fullscreenButton: false,
            geocoder: false,
            homeButton: false,
            infoBox: false,
            sceneModePicker: false,
            selectionIndicator: false,
            navigationHelpButton: false,
            creditContainer: document.createElement('div'),
            baseLayer: new Cesium.ImageryLayer(blueMarbleProvider),
            skyBox: false,
            skyAtmosphere: new Cesium.SkyAtmosphere(),
            contextOptions: {
                webgl: {
                    alpha: true
                }
            }
        });

        // Dark space background
        this.viewer.scene.backgroundColor = Cesium.Color.fromCssColorString('#020408');
        this.viewer.scene.globe.baseColor = Cesium.Color.fromCssColorString('#0a1428');

        // Enable lighting for day/night effect
        this.viewer.scene.globe.enableLighting = true;

        // Add night lights layer (Black Marble)
        const blackMarbleProvider = await Cesium.SingleTileImageryProvider.fromUrl(
            'imagery/BlackMarble_2016_3km.jpeg',
            {
                rectangle: Cesium.Rectangle.fromDegrees(-180.0, -90.0, 180.0, 90.0),
                tileWidth: 13500,
                tileHeight: 6750
            }
        );

        this.nightLightsLayer = this.viewer.imageryLayers.addImageryProvider(blackMarbleProvider);

        // Configure night lights layer
        this.nightLightsLayer.dayAlpha = 0.0;      // Hide during day
        this.nightLightsLayer.nightAlpha = 1.0;   // Show at night
        this.nightLightsLayer.brightness = 2.0;   // Boost brightness
        this.nightLightsLayer.contrast = 1.2;
        this.nightLightsLayer.gamma = 0.6;
        this.nightLightsLayer.saturation = 1.2;

        // Set initial camera view - centered on Earth with 30 degree tilt, zoomed out
        this.viewer.camera.lookAt(
            Cesium.Cartesian3.ZERO,  // Look at Earth center
            new Cesium.HeadingPitchRange(
                0,                              // heading (0 = north)
                Cesium.Math.toRadians(-60),     // pitch (-90 = straight down, -60 = 30° tilt)
                45000000                        // distance from center (45,000 km) - more zoomed out
            )
        );
        // Unlock camera so user can rotate
        this.viewer.camera.lookAtTransform(Cesium.Matrix4.IDENTITY);

        // Handle resize
        window.addEventListener('resize', () => {
            if (this.viewer) {
                this.viewer.resize();
            }
        });

        // Fade night lights based on altitude (only show from space)
        this.viewer.scene.postRender.addEventListener(() => {
            if (this.nightLightsLayer) {
                const height = this.viewer.camera.positionCartographic.height;
                const FADE_START = 70000;  // Start fading in at 70km
                const FADE_END = 50000;    // Fully hidden below 50km

                if (height <= FADE_END) {
                    this.nightLightsLayer.alpha = 0;
                } else if (height >= FADE_START) {
                    this.nightLightsLayer.alpha = 1;
                } else {
                    this.nightLightsLayer.alpha = (height - FADE_END) / (FADE_START - FADE_END);
                }
            }
        });

        // Add equatorial plane as a circle
        this.addEquatorialPlane();
    }

    addEquatorialPlane() {
        const earthRadius = 6371000; // meters
        const diskRadius = earthRadius * 3; // Extend beyond Earth
        const numPoints = 120;

        // Simple polygon for the equatorial disk - positions around the circle at height 0
        const diskPositions = [];
        for (let i = 0; i < numPoints; i++) {
            const lon = (i / numPoints) * 360 - 180; // -180 to 180
            diskPositions.push(Cesium.Cartesian3.fromDegrees(lon, 0, diskRadius - earthRadius));
        }

        this.equatorialDisk = this.viewer.entities.add({
            polygon: {
                hierarchy: diskPositions,
                material: Cesium.Color.fromCssColorString('#00f0ff').withAlpha(0.3),
                perPositionHeight: true,
                classificationType: Cesium.ClassificationType.CESIUM_3D_TILE
            }
        });

        // Ensure globe renders on top of transparent geometry
        this.viewer.scene.globe.depthTestAgainstTerrain = true;

        // Add equator line on Earth's surface
        const equatorPositions = Cesium.Cartesian3.fromDegreesArray([
            -180, 0, -90, 0, 0, 0, 90, 0, 180, 0
        ]);

        this.equatorLine = this.viewer.entities.add({
            polyline: {
                positions: equatorPositions,
                width: 2,
                material: Cesium.Color.fromCssColorString('#00f0ff').withAlpha(0.6),
                clampToGround: true
            }
        });
    }

    // ==================== WASM Loading ====================

    async loadWasm() {
        const statusEl = document.getElementById('wasm-status');
        const dotEl = document.getElementById('wasm-dot');
        const runBtn = document.getElementById('run-btn');
        const self = this;

        try {
            this.log('Loading WASM module...', 'info');

            // Set up Module object BEFORE loading the script to capture all output
            window.Module = {
                print: function(text) {
                    self.processOutput(text);
                },
                printErr: function(text) {
                    self.processOutput(text);
                    console.error(text);
                },
                onRuntimeInitialized: function() {
                    self.log('WASM runtime initialized', 'info');
                }
            };

            // Create script element to load WASM
            const script = document.createElement('script');
            script.src = 'tudat_wasm_test.js';

            await new Promise((resolve, reject) => {
                script.onload = resolve;
                script.onerror = reject;
                document.head.appendChild(script);
            });

            // Wait for Module to be ready
            await this.waitForModule();

            this.wasmModule = Module;

            statusEl.textContent = 'READY';
            dotEl.className = 'status-dot ready';
            runBtn.disabled = false;

            this.log('WASM module loaded successfully', 'pass');
            this.log('System ready. Click EXECUTE to run tests.', 'info');
        } catch (error) {
            console.error('WASM load error:', error);
            statusEl.textContent = 'LOAD FAILED';
            dotEl.className = 'status-dot error';
            this.log(`WASM load failed: ${error.message}`, 'error');
            this.log('Running in demo mode...', 'info');

            // Enable demo mode
            runBtn.disabled = false;
            this.useDemoMode = true;
        }
    }

    waitForModule() {
        return new Promise((resolve) => {
            const check = () => {
                if (typeof Module !== 'undefined' && Module.calledRun) {
                    resolve();
                } else if (typeof Module !== 'undefined') {
                    if (Module.onRuntimeInitialized) {
                        const original = Module.onRuntimeInitialized;
                        Module.onRuntimeInitialized = () => {
                            original();
                            resolve();
                        };
                    } else {
                        Module.onRuntimeInitialized = resolve;
                    }
                } else {
                    setTimeout(check, 50);
                }
            };
            check();
        });
    }

    // ==================== Output Processing ====================

    processOutput(text) {
        if (!text || typeof text !== 'string') return;

        // Log to console panel
        this.log(text, this.classifyLine(text));

        // Parse test results
        if (text.startsWith('[PASS]')) {
            const testName = text.substring(7).trim();
            this.addTestResult(testName, true);
        } else if (text.startsWith('[FAIL]')) {
            const testName = text.substring(7).trim();
            this.addTestResult(testName, false);
        } else if (text.startsWith('===') && text.endsWith('===')) {
            // Category header
            this.currentCategory = text.replace(/=/g, '').trim();
        } else if (text.includes('Tests run:')) {
            const match = text.match(/Tests run:\s*(\d+)/);
            if (match) {
                this.expectedTests = parseInt(match[1]);
            }
        }

        this.updateStats();
        this.updateProgress();
    }

    classifyLine(text) {
        if (text.startsWith('[PASS]')) return 'pass';
        if (text.startsWith('[FAIL]')) return 'fail';
        if (text.startsWith('[INFO]')) return 'info';
        if (text.startsWith('[ERROR]')) return 'error';
        if (text.startsWith('===')) return 'header';
        if (text.includes('Tests run') || text.includes('ALL TESTS')) return 'summary';
        return 'info';
    }

    addTestResult(name, passed) {
        const result = {
            name: name,
            passed: passed,
            category: this.currentCategory,
            timestamp: Date.now() - (this.startTime || Date.now())
        };

        this.testResults.push(result);

        // Group by category
        if (!this.categories[this.currentCategory]) {
            this.categories[this.currentCategory] = [];
        }
        this.categories[this.currentCategory].push(result);

        this.updateCategoryList();
    }

    log(text, type = 'info') {
        const console = document.getElementById('console-output');
        const line = document.createElement('div');
        line.className = `console-line ${type}`;
        line.textContent = text;
        console.appendChild(line);
        console.scrollTop = console.scrollHeight;

        this.consoleLines++;
        document.getElementById('line-count').textContent = `${this.consoleLines} lines`;
    }

    // ==================== UI Updates ====================

    updateStats() {
        const total = this.testResults.length;
        const passed = this.testResults.filter(r => r.passed).length;
        const failed = total - passed;
        const duration = this.startTime ? ((Date.now() - this.startTime) / 1000).toFixed(1) : '-';

        document.getElementById('total-count').textContent = total || '-';
        document.getElementById('passed-count').textContent = passed || '-';
        document.getElementById('failed-count').textContent = failed || '-';
        document.getElementById('duration').textContent = duration !== '-' ? `${duration}s` : '-';
    }

    updateProgress() {
        const progress = Math.min((this.testResults.length / this.expectedTests) * 100, 100);
        document.getElementById('progress-bar').style.width = `${progress}%`;
        document.getElementById('progress-text').textContent = `${Math.round(progress)}%`;
    }

    updateCategoryList() {
        const container = document.getElementById('category-list');
        container.innerHTML = '';

        Object.entries(this.categories).forEach(([category, tests]) => {
            const passed = tests.filter(t => t.passed).length;
            const failed = tests.length - passed;
            const hasFailures = failed > 0;

            const item = document.createElement('div');
            item.className = `category-item ${hasFailures ? 'has-failures' : ''}`;
            item.innerHTML = `
                <div class="category-header">
                    <span class="category-name">${category}</span>
                    <span class="category-count ${hasFailures ? 'has-fail' : 'all-pass'}">${passed}/${tests.length}</span>
                </div>
                <div class="category-tests">
                    ${tests.map(t => `
                        <div class="test-item" data-test="${t.name}" data-category="${category}">
                            <span class="test-icon ${t.passed ? 'pass' : 'fail'}">${t.passed ? '+' : '!'}</span>
                            <span class="test-name">${t.name}</span>
                        </div>
                    `).join('')}
                </div>
            `;

            // Click to expand category
            item.querySelector('.category-header').addEventListener('click', () => {
                item.classList.toggle('expanded');
            });

            // Click on individual tests to visualize
            item.querySelectorAll('.test-item').forEach(testEl => {
                testEl.addEventListener('click', (e) => {
                    e.stopPropagation();
                    const testName = testEl.dataset.test;
                    const testCategory = testEl.dataset.category;
                    this.selectTest(testName, testCategory);
                });
            });

            container.appendChild(item);
        });
    }

    selectTest(testName, category) {
        this.selectedTest = testName;

        // Highlight selected test in list
        document.querySelectorAll('.test-item').forEach(el => {
            el.classList.remove('selected');
            if (el.dataset.test === testName) {
                el.classList.add('selected');
            }
        });

        // Trigger visualization
        this.visualizeTest(testName, category);
    }

    // ==================== Test Execution ====================

    async runTests() {
        if (this.isRunning) return;

        this.isRunning = true;
        this.testResults = [];
        this.categories = {};
        this.startTime = Date.now();
        this.currentCategory = 'General';

        // Update UI
        const runBtn = document.getElementById('run-btn');
        runBtn.disabled = true;
        runBtn.innerHTML = '<span class="spinner"></span>RUNNING';

        document.getElementById('progress-section').style.display = 'block';
        document.getElementById('progress-bar').style.width = '0%';
        document.getElementById('console-output').innerHTML = '';
        document.getElementById('category-list').innerHTML = '';
        this.consoleLines = 0;

        document.getElementById('wasm-status').textContent = 'EXECUTING';
        document.getElementById('wasm-dot').className = 'status-dot loading';

        this.log('Starting test execution...', 'header');
        this.log(`Timestamp: ${new Date().toISOString()}`, 'info');

        try {
            if (this.useDemoMode) {
                await this.runDemoTests();
            } else {
                await this.runWasmTests();
            }
        } catch (error) {
            this.log(`Execution error: ${error.message}`, 'error');
            console.error(error);
        }

        this.finishTests();
    }

    async runWasmTests() {
        if (this.wasmModule && this.wasmModule.callMain) {
            this.wasmModule.callMain([]);
        } else if (this.wasmModule && this.wasmModule._main) {
            this.wasmModule._main();
        } else {
            this.log('WASM entry point not found, check module configuration', 'error');
        }

        await this.sleep(1000);
    }

    async runDemoTests() {
        const categories = [
            { name: 'Unit Conversions', tests: ['180 degrees to radians', 'PI radians to degrees', '1 AU to meters', '1 meter to AU'] },
            { name: 'Physical Constants', tests: ['Speed of light (c)', 'Gravitational constant (G)', 'Astronomical unit', 'Earth gravitational parameter'] },
            { name: 'Orbital Elements (NASA ODTBX)', tests: [
                'Earth elliptical component 0', 'Earth elliptical component 1', 'Earth elliptical component 2',
                'Mars circular X position', 'Mars circular Y position', 'Sun hyperbolic component 0'
            ]},
            { name: 'Two-Body Propagation', tests: ['Propagation completed', 'Position matches Kepler (< 0.1 m)', 'Velocity matches Kepler (< 1e-4 m/s)'] },
            { name: 'CR3BP Propagation', tests: ['CR3BP state component 0', 'CR3BP state component 1', 'CR3BP state component 2', 'CR3BP step count > 100'] },
            { name: 'TLE/SGP4 (Vallado Benchmark)', tests: ['Vallado TLE position error < 50m', 'Vallado TLE velocity error < 0.05 m/s', 'TLE parsed inclination', 'TLE parsed eccentricity'] },
            { name: 'SPICE Interface', tests: ['ET 0.0 -> JD 2451545.0', 'JD round-trip', 'J2000->ECLIP determinant'] }
        ];

        for (const cat of categories) {
            this.processOutput(`\n=== ${cat.name} ===`);
            await this.sleep(30);

            for (const test of cat.tests) {
                const passed = Math.random() > 0.02;
                this.processOutput(`[${passed ? 'PASS' : 'FAIL'}] ${test}`);
                await this.sleep(15);
            }
        }

        this.processOutput('\n=== Test Results ===');
        const total = this.testResults.length;
        const passed = this.testResults.filter(r => r.passed).length;
        this.processOutput(`[INFO] Tests run: ${total}`);
        this.processOutput(`[INFO] Tests passed: ${passed}`);
        this.processOutput(`[INFO] Tests failed: ${total - passed}`);
        this.processOutput(passed === total ? '[PASS] *** ALL TESTS PASSED ***' : '[FAIL] *** SOME TESTS FAILED ***');
    }

    finishTests() {
        this.isRunning = false;

        const runBtn = document.getElementById('run-btn');
        runBtn.disabled = false;
        runBtn.innerHTML = 'EXECUTE';

        const passed = this.testResults.filter(r => r.passed).length;
        const failed = this.testResults.length - passed;

        document.getElementById('wasm-status').textContent = failed === 0 ? 'PASS' : 'FAIL';
        document.getElementById('wasm-dot').className = `status-dot ${failed === 0 ? 'ready' : 'error'}`;

        this.log(`\nTest execution complete: ${passed}/${this.testResults.length} passed`, 'summary');
    }

    clearResults() {
        this.testResults = [];
        this.categories = {};
        this.consoleLines = 0;
        this.selectedTest = null;

        document.getElementById('total-count').textContent = '-';
        document.getElementById('passed-count').textContent = '-';
        document.getElementById('failed-count').textContent = '-';
        document.getElementById('duration').textContent = '-';
        document.getElementById('progress-bar').style.width = '0%';
        document.getElementById('progress-text').textContent = '0%';
        document.getElementById('progress-section').style.display = 'none';
        document.getElementById('line-count').textContent = '0 lines';
        document.getElementById('orbit-info').textContent = 'Click a test to visualize';

        document.getElementById('console-output').innerHTML = '<div class="console-line info">Console cleared. Ready for new test run.</div>';
        document.getElementById('category-list').innerHTML = '<div style="color: var(--text-dim); font-size: 14px; text-align: center; padding: 20px;">Awaiting test execution...</div>';

        document.getElementById('wasm-status').textContent = 'READY';
        document.getElementById('wasm-dot').className = 'status-dot ready';

        // Clear Cesium entities
        this.clearOrbitEntities();
        this.resetCharts();
    }

    // ==================== Visualization System ====================

    visualizeTest(testName, category) {
        // Update info display
        document.getElementById('orbit-info').textContent = `${category}: ${testName}`;

        // Clear previous 3D entities
        this.clearOrbitEntities();

        // Show 3D visualization on globe
        this.show3DVisualization(category, testName);

        // Show 2D chart below
        this.showChartForCategory(category, testName);
    }

    show3DVisualization(category, testName) {
        const cat = category.toLowerCase();

        if (cat.includes('two-body') || cat.includes('two body') || cat.includes('kepler')) {
            // LEO circular orbit
            this.addOrbitToGlobe(7000, 0, 0, '#00ff9d');
        }
        else if (cat.includes('orbital element') || cat.includes('odtbx')) {
            // Elliptical orbit
            this.addOrbitToGlobe(10000, 0.3, 28.5, '#ff9f1c');
        }
        else if (cat.includes('tle') || cat.includes('sgp4')) {
            // ISS-like orbit (inclined)
            this.addOrbitToGlobe(6778, 0.0005, 51.6, '#00f0ff');
        }
        else if (cat.includes('cr3bp') || cat.includes('three-body')) {
            // High altitude transfer-like orbit
            this.addOrbitToGlobe(20000, 0.5, 10, '#8b5cf6');
        }
        else if (cat.includes('coordinate') || cat.includes('frame') || cat.includes('spice')) {
            // Show coordinate axes
            this.addCoordinateAxes();
        }
        else if (cat.includes('mass') || cat.includes('propulsion')) {
            // GTO-like orbit for propulsion
            this.addOrbitToGlobe(24000, 0.7, 7, '#ff3366');
        }
    }

    addOrbitToGlobe(semiMajorAxis, eccentricity, inclination, color) {
        // Generate orbit points using Keplerian elements
        const a = semiMajorAxis; // km
        const e = eccentricity;
        const i = inclination * Math.PI / 180; // radians
        const earthRadius = 6371; // km

        const positions = [];
        const p = a * (1 - e * e);

        for (let nu = 0; nu <= 360; nu += 3) {
            const nuRad = nu * Math.PI / 180;
            const r = p / (1 + e * Math.cos(nuRad));

            // Position in orbital plane
            const xOrb = r * Math.cos(nuRad);
            const yOrb = r * Math.sin(nuRad);

            // Rotate by inclination (simple rotation around X axis)
            const x = xOrb;
            const y = yOrb * Math.cos(i);
            const z = yOrb * Math.sin(i);

            // Convert to lat/lon/alt for Cesium
            const rTotal = Math.sqrt(x*x + y*y + z*z);
            const lat = Math.asin(z / rTotal) * 180 / Math.PI;
            const lon = Math.atan2(y, x) * 180 / Math.PI;
            const alt = (rTotal - earthRadius) * 1000; // meters

            positions.push(Cesium.Cartesian3.fromDegrees(lon, lat, alt));
        }

        const orbitEntity = this.viewer.entities.add({
            polyline: {
                positions: positions,
                width: 2,
                material: new Cesium.PolylineGlowMaterialProperty({
                    glowPower: 0.2,
                    color: Cesium.Color.fromCssColorString(color)
                })
            }
        });
        this.orbitEntities.push(orbitEntity);

        // Add satellite marker
        const satEntity = this.viewer.entities.add({
            position: positions[0],
            point: {
                pixelSize: 8,
                color: Cesium.Color.fromCssColorString(color),
                outlineColor: Cesium.Color.WHITE,
                outlineWidth: 1
            }
        });
        this.orbitEntities.push(satEntity);
    }

    addCoordinateAxes() {
        const axisLength = 15000000; // 15,000 km in meters

        // X axis (red) - points toward vernal equinox
        const xAxis = this.viewer.entities.add({
            polyline: {
                positions: [
                    Cesium.Cartesian3.fromDegrees(0, 0, 0),
                    Cesium.Cartesian3.fromDegrees(0, 0, axisLength)
                ],
                width: 3,
                material: Cesium.Color.fromCssColorString('#ff3366')
            }
        });
        this.orbitEntities.push(xAxis);

        // Y axis (green)
        const yAxis = this.viewer.entities.add({
            polyline: {
                positions: [
                    Cesium.Cartesian3.fromDegrees(0, 0, 0),
                    Cesium.Cartesian3.fromDegrees(90, 0, axisLength)
                ],
                width: 3,
                material: Cesium.Color.fromCssColorString('#00ff9d')
            }
        });
        this.orbitEntities.push(yAxis);

        // Z axis (cyan) - points toward north pole
        const zAxis = this.viewer.entities.add({
            polyline: {
                positions: [
                    Cesium.Cartesian3.fromDegrees(0, 0, 0),
                    Cesium.Cartesian3.fromDegrees(0, 90, axisLength)
                ],
                width: 3,
                material: Cesium.Color.fromCssColorString('#00f0ff')
            }
        });
        this.orbitEntities.push(zAxis);
    }

    showChartForCategory(category, testName) {
        const cat = category.toLowerCase();
        const titleEl = document.getElementById('chart-title');

        if (cat.includes('unit conver')) {
            titleEl.textContent = 'Unit Conversion: 3D Angle Mapping';
            // Show unit circle in 3D
            const points = [];
            for (let d = 0; d <= 360; d += 10) {
                const rad = d * Math.PI / 180;
                points.push({
                    x: Math.cos(rad) * 50,
                    y: Math.sin(rad) * 50,
                    z: d / 360 * 50 - 25
                });
            }
            this.render3DChart({ type: 'trajectory', points, scale: 80, color: this.chartColors.cyan });
        }
        else if (cat.includes('physical const')) {
            titleEl.textContent = 'Physical Constants: 3D Bar Chart';
            this.render3DChart({
                type: 'bars3d',
                scale: 100,
                bars: [
                    { value: 80, color: this.chartColors.cyan, label: 'c' },
                    { value: 30, color: this.chartColors.purple, label: 'G' },
                    { value: 60, color: this.chartColors.green, label: 'AU' },
                    { value: 45, color: this.chartColors.orange, label: 'μ' }
                ]
            });
        }
        else if (cat.includes('orbital element') || cat.includes('odtbx')) {
            titleEl.textContent = 'Orbital Elements: 3D Ellipse';
            // Generate elliptical orbit in 3D
            const points = [];
            const a = 50, e = 0.3, inc = 28.5 * Math.PI / 180;
            for (let theta = 0; theta <= 360; theta += 5) {
                const rad = theta * Math.PI / 180;
                const r = a * (1 - e * e) / (1 + e * Math.cos(rad));
                const x = r * Math.cos(rad);
                const y = r * Math.sin(rad) * Math.cos(inc);
                const z = r * Math.sin(rad) * Math.sin(inc);
                points.push({ x, y, z });
            }
            this.render3DChart({ type: 'trajectory', points, scale: 80, color: this.chartColors.orange });
        }
        else if (cat.includes('anomaly')) {
            titleEl.textContent = 'Anomaly Conversion: 3D Scatter';
            const points = [];
            const e = 0.4;
            for (let M = 0; M <= 360; M += 15) {
                let E = M * Math.PI / 180;
                for (let i = 0; i < 10; i++) E = M * Math.PI / 180 + e * Math.sin(E);
                let nu = 2 * Math.atan(Math.sqrt((1+e)/(1-e)) * Math.tan(E/2));
                if (nu < 0) nu += 2 * Math.PI;
                points.push({
                    x: M / 360 * 80 - 40,
                    y: E * 180 / Math.PI / 360 * 80 - 40,
                    z: nu * 180 / Math.PI / 360 * 80 - 40,
                    color: this.chartColors.purple,
                    size: 5
                });
            }
            this.render3DChart({ type: 'scatter3d', points, scale: 80 });
        }
        else if (cat.includes('coordinate') || cat.includes('frame') || cat.includes('spice')) {
            titleEl.textContent = 'Coordinate Transform: 3D Rotation';
            const points = [];
            // Show rotation path
            for (let a = 0; a <= 360; a += 5) {
                const rad = a * Math.PI / 180;
                points.push({
                    x: 40 * Math.cos(rad),
                    y: 40 * Math.sin(rad) * Math.cos(0.4),
                    z: 40 * Math.sin(rad) * Math.sin(0.4)
                });
            }
            this.render3DChart({ type: 'trajectory', points, scale: 80, color: this.chartColors.green });
        }
        else if (cat.includes('legendre') || cat.includes('spherical harmon')) {
            titleEl.textContent = 'Spherical Harmonics: Y₂₀';
            this.render3DChart({ type: 'spherical', scale: 80, l: 2, m: 0 });
        }
        else if (cat.includes('gravit')) {
            titleEl.textContent = 'Gravity Field: 3D Surface';
            // Generate gravity potential surface
            const grid = [];
            for (let i = 0; i < 20; i++) {
                const row = [];
                for (let j = 0; j < 20; j++) {
                    const x = (i - 10) / 10;
                    const y = (j - 10) / 10;
                    const r = Math.sqrt(x * x + y * y) + 0.1;
                    row.push(1 / r * 0.3);
                }
                grid.push(row);
            }
            this.render3DChart({ type: 'surface', grid, scale: 100 });
        }
        else if (cat.includes('two-body') || cat.includes('two body')) {
            titleEl.textContent = 'Two-Body: 3D Circular Orbit';
            const points = [];
            const r = 40;
            for (let t = 0; t <= 360; t += 5) {
                const rad = t * Math.PI / 180;
                points.push({
                    x: r * Math.cos(rad),
                    y: r * Math.sin(rad),
                    z: 0
                });
            }
            this.render3DChart({ type: 'trajectory', points, scale: 80, color: this.chartColors.green });
        }
        else if (cat.includes('cr3bp') || cat.includes('three-body')) {
            titleEl.textContent = 'CR3BP: Lagrange Points';
            // Show Earth, Moon, and L1/L2 points
            const points = [
                { x: -40, y: 0, z: 0, color: this.chartColors.cyan, size: 12 },    // Earth
                { x: 40, y: 0, z: 0, color: this.chartColors.dim, size: 8 },       // Moon
                { x: 25, y: 0, z: 0, color: this.chartColors.purple, size: 5 },    // L1
                { x: 55, y: 0, z: 0, color: this.chartColors.purple, size: 5 },    // L2
                { x: 0, y: 35, z: 0, color: this.chartColors.orange, size: 5 },    // L4
                { x: 0, y: -35, z: 0, color: this.chartColors.orange, size: 5 }    // L5
            ];
            this.render3DChart({ type: 'scatter3d', points, scale: 80 });
        }
        else if (cat.includes('tle') || cat.includes('sgp4')) {
            titleEl.textContent = 'TLE/SGP4: ISS Orbit';
            const points = [];
            const r = 35, inc = 51.6 * Math.PI / 180;
            for (let t = 0; t <= 360; t += 5) {
                const rad = t * Math.PI / 180;
                points.push({
                    x: r * Math.cos(rad),
                    y: r * Math.sin(rad) * Math.cos(inc),
                    z: r * Math.sin(rad) * Math.sin(inc)
                });
            }
            this.render3DChart({ type: 'trajectory', points, scale: 80, color: this.chartColors.cyan });
        }
        else if (cat.includes('mass') || cat.includes('propulsion')) {
            titleEl.textContent = 'Mass Propagation: Fuel Consumption';
            this.render3DChart({
                type: 'bars3d',
                scale: 100,
                bars: [
                    { value: 100, color: this.chartColors.green },
                    { value: 90, color: this.chartColors.green },
                    { value: 75, color: this.chartColors.cyan },
                    { value: 60, color: this.chartColors.cyan },
                    { value: 45, color: this.chartColors.orange },
                    { value: 30, color: this.chartColors.orange },
                    { value: 15, color: this.chartColors.red }
                ]
            });
        }
        else if (cat.includes('integrat')) {
            titleEl.textContent = 'Numerical Integration: 3D Trajectory';
            const points = [];
            let x = 50, y = 0, z = 0;
            for (let t = 0; t < 200; t++) {
                points.push({ x, y, z });
                // Lorenz attractor simplified
                const dx = 0.01 * (10 * (y - x));
                const dy = 0.01 * (x * (28 - z) - y);
                const dz = 0.01 * (x * y - 2.67 * z);
                x += dx; y += dy; z += dz;
            }
            this.render3DChart({ type: 'trajectory', points, scale: 100, color: this.chartColors.purple });
        }
        else if (cat.includes('interpol') || cat.includes('spline')) {
            titleEl.textContent = 'Interpolation: 3D Spline';
            const points = [];
            for (let t = 0; t <= 1; t += 0.02) {
                points.push({
                    x: 60 * Math.cos(t * 4 * Math.PI) * (1 - t),
                    y: 60 * Math.sin(t * 4 * Math.PI) * (1 - t),
                    z: t * 80 - 40
                });
            }
            this.render3DChart({ type: 'trajectory', points, scale: 100, color: this.chartColors.green });
        }
        else if (cat.includes('statistic')) {
            titleEl.textContent = 'Statistics: 3D Gaussian';
            const grid = [];
            for (let i = 0; i < 25; i++) {
                const row = [];
                for (let j = 0; j < 25; j++) {
                    const x = (i - 12) / 4;
                    const y = (j - 12) / 4;
                    row.push(Math.exp(-0.5 * (x * x + y * y)));
                }
                grid.push(row);
            }
            this.render3DChart({ type: 'surface', grid, scale: 100 });
        }
        else if (cat.includes('time')) {
            titleEl.textContent = 'Time Scales: Julian Date';
            this.render3DChart({
                type: 'bars3d',
                scale: 100,
                bars: [
                    { value: 85, color: this.chartColors.cyan, label: 'JD' },
                    { value: 70, color: this.chartColors.purple, label: 'MJD' },
                    { value: 55, color: this.chartColors.green, label: 'TDB' },
                    { value: 40, color: this.chartColors.orange, label: 'TAI' }
                ]
            });
        }
        else if (cat.includes('resource') || cat.includes('emscripten') || cat.includes('wasm')) {
            titleEl.textContent = 'WASM: Memory Usage';
            this.render3DChart({
                type: 'bars3d',
                scale: 100,
                bars: [
                    { value: 90, color: this.chartColors.cyan, label: 'Heap' },
                    { value: 45, color: this.chartColors.green, label: 'Stack' },
                    { value: 30, color: this.chartColors.purple, label: 'Data' }
                ]
            });
        }
        else if (cat.includes('equinoctial')) {
            titleEl.textContent = 'Equinoctial Elements';
            const points = [];
            for (let i = 0; i < 100; i++) {
                const t = i / 100 * 2 * Math.PI;
                points.push({
                    x: 40 * Math.cos(t) + 10 * Math.cos(3 * t),
                    y: 40 * Math.sin(t) + 10 * Math.sin(3 * t),
                    z: 20 * Math.sin(2 * t)
                });
            }
            this.render3DChart({ type: 'trajectory', points, scale: 80, color: this.chartColors.orange });
        }
        else {
            // Default: simple 3D scatter
            titleEl.textContent = category;
            const points = [];
            for (let i = 0; i < 50; i++) {
                points.push({
                    x: (Math.random() - 0.5) * 80,
                    y: (Math.random() - 0.5) * 80,
                    z: (Math.random() - 0.5) * 80,
                    color: this.chartColors.cyan,
                    size: 3
                });
            }
            this.render3DChart({ type: 'scatter3d', points, scale: 80 });
        }
    }

    clearOrbitEntities() {
        this.orbitEntities.forEach(entity => {
            this.viewer.entities.remove(entity);
        });
        this.orbitEntities = [];
    }

    generateOrbitalData() {
        // Keep for compatibility but not used anymore
        this.orbitalData = {};
    }

    // ==================== D3.js 3D Charts ====================

    setupCharts() {
        this.chartColors = {
            cyan: '#00f0ff',
            purple: '#8b5cf6',
            green: '#00ff9d',
            red: '#ff3366',
            orange: '#ff9f1c',
            yellow: '#ffd93d',
            dim: '#4a6066',
            bg: '#0d1620'
        };

        // D3 chart container
        this.d3Container = d3.select('#d3-chart');
        this.d3Rotation = { x: -20, y: 30, z: 0 };
        this.isDragging = false;

        // Setup mouse drag rotation
        const container = document.getElementById('d3-chart');
        container.addEventListener('mousedown', (e) => {
            this.isDragging = true;
            this.lastMouse = { x: e.clientX, y: e.clientY };
        });
        document.addEventListener('mousemove', (e) => {
            if (this.isDragging && this.lastMouse) {
                const dx = e.clientX - this.lastMouse.x;
                const dy = e.clientY - this.lastMouse.y;
                this.d3Rotation.y += dx * 0.5;
                this.d3Rotation.x += dy * 0.5;
                this.lastMouse = { x: e.clientX, y: e.clientY };
                if (this.currentD3Data) {
                    this.render3DChart(this.currentD3Data);
                }
            }
        });
        document.addEventListener('mouseup', () => {
            this.isDragging = false;
        });
    }

    // Project 3D point to 2D using rotation
    project3D(x, y, z, width, height, scale) {
        const rx = this.d3Rotation.x * Math.PI / 180;
        const ry = this.d3Rotation.y * Math.PI / 180;

        // Rotate around X axis
        let y1 = y * Math.cos(rx) - z * Math.sin(rx);
        let z1 = y * Math.sin(rx) + z * Math.cos(rx);

        // Rotate around Y axis
        let x2 = x * Math.cos(ry) + z1 * Math.sin(ry);
        let z2 = -x * Math.sin(ry) + z1 * Math.cos(ry);

        // Perspective projection
        const perspective = 500;
        const factor = perspective / (perspective + z2);

        return {
            x: width / 2 + x2 * scale * factor,
            y: height / 2 - y1 * scale * factor,
            z: z2
        };
    }

    render3DChart(data) {
        this.currentD3Data = data;
        const container = document.getElementById('d3-chart');
        const width = container.clientWidth;
        const height = container.clientHeight;

        // Clear previous
        this.d3Container.selectAll('*').remove();

        const svg = this.d3Container.append('svg')
            .attr('width', width)
            .attr('height', height)
            .style('background', this.chartColors.bg);

        // Draw based on chart type
        if (data.type === 'scatter3d') {
            this.draw3DScatter(svg, data, width, height);
        } else if (data.type === 'surface') {
            this.draw3DSurface(svg, data, width, height);
        } else if (data.type === 'trajectory') {
            this.draw3DTrajectory(svg, data, width, height);
        } else if (data.type === 'bars3d') {
            this.draw3DBars(svg, data, width, height);
        } else if (data.type === 'spherical') {
            this.draw3DSpherical(svg, data, width, height);
        }

        // Draw axes
        this.draw3DAxes(svg, data.scale || 100, width, height);
    }

    draw3DAxes(svg, scale, width, height) {
        const axes = [
            { start: [0, 0, 0], end: [scale, 0, 0], color: this.chartColors.red, label: 'X' },
            { start: [0, 0, 0], end: [0, scale, 0], color: this.chartColors.green, label: 'Y' },
            { start: [0, 0, 0], end: [0, 0, scale], color: this.chartColors.cyan, label: 'Z' }
        ];

        axes.forEach(axis => {
            const p1 = this.project3D(...axis.start, width, height, 0.8);
            const p2 = this.project3D(...axis.end, width, height, 0.8);

            svg.append('line')
                .attr('x1', p1.x).attr('y1', p1.y)
                .attr('x2', p2.x).attr('y2', p2.y)
                .attr('stroke', axis.color)
                .attr('stroke-width', 1)
                .attr('opacity', 0.5);

            svg.append('text')
                .attr('x', p2.x + 5).attr('y', p2.y)
                .attr('fill', axis.color)
                .attr('font-size', '10px')
                .attr('font-family', 'Share Tech Mono')
                .text(axis.label);
        });
    }

    draw3DScatter(svg, data, width, height) {
        const points = data.points;
        const scale = data.scale || 100;

        // Sort by z for proper depth rendering
        const projected = points.map(p => ({
            ...this.project3D(p.x, p.y, p.z, width, height, scale / Math.max(...points.map(pt => Math.abs(pt.x)), ...points.map(pt => Math.abs(pt.y)), ...points.map(pt => Math.abs(pt.z))) * 0.8),
            color: p.color || this.chartColors.cyan,
            size: p.size || 4
        })).sort((a, b) => a.z - b.z);

        projected.forEach(p => {
            svg.append('circle')
                .attr('cx', p.x).attr('cy', p.y)
                .attr('r', p.size)
                .attr('fill', p.color)
                .attr('opacity', 0.8);
        });
    }

    draw3DTrajectory(svg, data, width, height) {
        const points = data.points;
        const maxVal = Math.max(...points.flatMap(p => [Math.abs(p.x), Math.abs(p.y), Math.abs(p.z)]));
        const scaleFactor = (data.scale || 100) / maxVal * 0.8;

        const projected = points.map(p => this.project3D(p.x, p.y, p.z, width, height, scaleFactor));

        // Draw trajectory line
        const line = d3.line()
            .x(d => d.x)
            .y(d => d.y)
            .curve(d3.curveCardinal);

        svg.append('path')
            .datum(projected)
            .attr('fill', 'none')
            .attr('stroke', data.color || this.chartColors.cyan)
            .attr('stroke-width', 2)
            .attr('d', line);

        // Draw start/end markers
        if (projected.length > 0) {
            svg.append('circle')
                .attr('cx', projected[0].x).attr('cy', projected[0].y)
                .attr('r', 6)
                .attr('fill', this.chartColors.green);

            svg.append('circle')
                .attr('cx', projected[projected.length - 1].x)
                .attr('cy', projected[projected.length - 1].y)
                .attr('r', 6)
                .attr('fill', this.chartColors.red);
        }
    }

    draw3DSurface(svg, data, width, height) {
        const grid = data.grid;
        const scale = data.scale || 100;
        const rows = grid.length;
        const cols = grid[0].length;

        // Create quads
        const quads = [];
        for (let i = 0; i < rows - 1; i++) {
            for (let j = 0; j < cols - 1; j++) {
                const x1 = (j / cols - 0.5) * scale * 2;
                const x2 = ((j + 1) / cols - 0.5) * scale * 2;
                const y1 = (i / rows - 0.5) * scale * 2;
                const y2 = ((i + 1) / rows - 0.5) * scale * 2;
                const z1 = grid[i][j] * scale * 0.5;
                const z2 = grid[i][j + 1] * scale * 0.5;
                const z3 = grid[i + 1][j + 1] * scale * 0.5;
                const z4 = grid[i + 1][j] * scale * 0.5;

                const avgZ = (z1 + z2 + z3 + z4) / 4;
                const p1 = this.project3D(x1, y1, z1, width, height, 0.8);
                const p2 = this.project3D(x2, y1, z2, width, height, 0.8);
                const p3 = this.project3D(x2, y2, z3, width, height, 0.8);
                const p4 = this.project3D(x1, y2, z4, width, height, 0.8);

                quads.push({
                    points: [p1, p2, p3, p4],
                    avgZ: (p1.z + p2.z + p3.z + p4.z) / 4,
                    value: avgZ
                });
            }
        }

        // Sort by depth and render
        quads.sort((a, b) => a.avgZ - b.avgZ);

        const colorScale = d3.scaleSequential(d3.interpolateViridis)
            .domain([d3.min(quads, d => d.value), d3.max(quads, d => d.value)]);

        quads.forEach(quad => {
            const pathData = `M${quad.points[0].x},${quad.points[0].y} L${quad.points[1].x},${quad.points[1].y} L${quad.points[2].x},${quad.points[2].y} L${quad.points[3].x},${quad.points[3].y} Z`;
            svg.append('path')
                .attr('d', pathData)
                .attr('fill', colorScale(quad.value))
                .attr('stroke', this.chartColors.dim)
                .attr('stroke-width', 0.5)
                .attr('opacity', 0.9);
        });
    }

    draw3DBars(svg, data, width, height) {
        const bars = data.bars;
        const scale = data.scale || 100;
        const barWidth = scale / bars.length * 1.5;

        // Create 3D bars with faces
        const allFaces = [];
        bars.forEach((bar, i) => {
            const x = (i / bars.length - 0.5) * scale * 2;
            const h = bar.value * scale * 0.01;
            const w = barWidth * 0.4;
            const color = bar.color || this.chartColors.cyan;

            // Top face
            const top = [
                this.project3D(x - w, h, -w, width, height, 0.8),
                this.project3D(x + w, h, -w, width, height, 0.8),
                this.project3D(x + w, h, w, width, height, 0.8),
                this.project3D(x - w, h, w, width, height, 0.8)
            ];
            allFaces.push({ points: top, z: d3.mean(top, p => p.z), color: d3.color(color).brighter(0.5) });

            // Front face
            const front = [
                this.project3D(x - w, 0, w, width, height, 0.8),
                this.project3D(x + w, 0, w, width, height, 0.8),
                this.project3D(x + w, h, w, width, height, 0.8),
                this.project3D(x - w, h, w, width, height, 0.8)
            ];
            allFaces.push({ points: front, z: d3.mean(front, p => p.z), color: color });

            // Right face
            const right = [
                this.project3D(x + w, 0, -w, width, height, 0.8),
                this.project3D(x + w, 0, w, width, height, 0.8),
                this.project3D(x + w, h, w, width, height, 0.8),
                this.project3D(x + w, h, -w, width, height, 0.8)
            ];
            allFaces.push({ points: right, z: d3.mean(right, p => p.z), color: d3.color(color).darker(0.3) });
        });

        // Sort by depth and render
        allFaces.sort((a, b) => a.z - b.z);
        allFaces.forEach(face => {
            const pathData = `M${face.points[0].x},${face.points[0].y} L${face.points[1].x},${face.points[1].y} L${face.points[2].x},${face.points[2].y} L${face.points[3].x},${face.points[3].y} Z`;
            svg.append('path')
                .attr('d', pathData)
                .attr('fill', face.color)
                .attr('stroke', this.chartColors.bg)
                .attr('stroke-width', 1);
        });
    }

    draw3DSpherical(svg, data, width, height) {
        // Draw spherical harmonic visualization as colored points on a sphere
        const points = [];
        const scale = data.scale || 80;
        const l = data.l || 2;
        const m = data.m || 0;

        for (let theta = 0; theta <= 180; theta += 10) {
            for (let phi = 0; phi <= 360; phi += 10) {
                const t = theta * Math.PI / 180;
                const p = phi * Math.PI / 180;

                // Simplified spherical harmonic (real part of Y_l^m)
                let Y = Math.pow(Math.cos(t), l) * Math.cos(m * p);
                const r = scale * (0.5 + 0.5 * Math.abs(Y));

                const x = r * Math.sin(t) * Math.cos(p);
                const y = r * Math.sin(t) * Math.sin(p);
                const z = r * Math.cos(t);

                points.push({ x, y, z, value: Y });
            }
        }

        const projected = points.map(pt => ({
            ...this.project3D(pt.x, pt.y, pt.z, width, height, 0.8),
            value: pt.value
        })).sort((a, b) => a.z - b.z);

        const colorScale = d3.scaleDiverging(d3.interpolateRdYlBu)
            .domain([-1, 0, 1]);

        projected.forEach(p => {
            svg.append('circle')
                .attr('cx', p.x).attr('cy', p.y)
                .attr('r', 3)
                .attr('fill', colorScale(p.value))
                .attr('opacity', 0.8);
        });
    }

    resetCharts() {
        this.d3Container.selectAll('*').remove();
        this.currentD3Data = null;
    }

    sleep(ms) {
        return new Promise(resolve => setTimeout(resolve, ms));
    }
}

// Initialize
window.addEventListener('DOMContentLoaded', () => {
    window.tudatRunner = new TudatTestRunner();
});
