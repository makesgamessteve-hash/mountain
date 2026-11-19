# quality-and-delivery Specification Delta

## Purpose

Defines the quality gates and delivery rules of the project: engine unit tests with a coverage threshold, automated game tests that drive the real game, continuous builds for every target, the published web build on `gh-pages`, and the code and asset licences.

## ADDED Requirements

### Requirement: Unit tests with coverage gate
The engine SHALL have automated unit tests, and continuous integration MUST fail when the line coverage of the engine's own source files, excluding third-party code, is below 95 percent.

#### Scenario: Coverage below threshold
- **GIVEN** a change that lowers engine line coverage to 94 percent
- **WHEN** continuous integration runs
- **THEN** the build is marked as failed and reports the coverage figure

#### Scenario: Third-party code excluded
- **WHEN** coverage is measured
- **THEN** vendored third-party sources are not counted in the coverage figure

### Requirement: Automated game tests
Automated game tests SHALL run the real game without a physical display or GPU, driving it with scripted input events and checking script assertions. They MUST cover walking, summoning the house, opening the door, entering rooms, taking a photo, weather, season and time changes, and touch and gamepad input. A test fails on any assertion failure, script error, crash or timeout.

#### Scenario: Passing scenario
- **WHEN** the game test for summoning the house, opening the door and walking into room 1 runs in continuous integration
- **THEN** it exits successfully after its assertions pass

#### Scenario: Failing assertion
- **GIVEN** a game test that asserts the door is open when it is closed
- **WHEN** the test runs
- **THEN** the test run fails and reports the failed assertion with its script location

#### Scenario: Crash is a failure
- **WHEN** the game crashes during a game test
- **THEN** the test run fails

#### Scenario: Touch and gamepad covered
- **WHEN** the game test suite runs
- **THEN** it includes scenarios that move the player with simulated touch input and with a simulated gamepad

### Requirement: Web build smoke test
Continuous integration SHALL load the web build in a headless browser and check that it starts, renders frames, switches to touch controls on a simulated touch event and offers "Enter VR" when an emulated immersive VR device is present.

#### Scenario: Web build starts
- **WHEN** the web smoke test runs in continuous integration
- **THEN** it passes only if the game starts in the headless browser without script or program errors

#### Scenario: Emulated VR
- **GIVEN** the headless browser provides an emulated immersive VR device
- **WHEN** the web build is loaded
- **THEN** the "Enter VR" button is offered and a VR session can be started

### Requirement: Continuous builds for all targets
Every push to the `main` branch and every pull request SHALL build the Linux, Windows and Web targets and run the unit tests and game tests in continuous integration, and the result MUST be visible on the commit.

#### Scenario: Push to main
- **WHEN** a commit is pushed to `main`
- **THEN** Linux, Windows and Web builds are produced and the tests run

#### Scenario: Broken Windows build
- **WHEN** a commit breaks the Windows build
- **THEN** the continuous integration run for that commit fails

### Requirement: Web build published to gh-pages
After a successful continuous-integration run on `main`, the web build SHALL be published to the `gh-pages` branch for playtesting. The `gh-pages` branch MUST contain only the files of the web build (page, program, asset package and their licence notices), never source code.

#### Scenario: Publishing
- **WHEN** continuous integration succeeds on a push to `main`
- **THEN** the `gh-pages` branch is updated with the new web build and the playtest page serves it

#### Scenario: Branch contents
- **WHEN** the files of the `gh-pages` branch are listed
- **THEN** they consist only of the web build files and licence notices, without engine sources, tests or build scripts

#### Scenario: Failed run does not publish
- **WHEN** a continuous-integration run on `main` fails
- **THEN** the `gh-pages` branch is not updated

### Requirement: Licensing
The repository SHALL declare the MIT licence for all code and the Creative Commons Attribution 4.0 (CC BY 4.0) licence for art and sound assets, each in a licence file, together with the attributions and licences of third-party components. The web build MUST include these notices.

#### Scenario: Licence files present
- **WHEN** the repository root and the asset folder are inspected
- **THEN** an MIT licence covers the code, a CC BY 4.0 declaration covers the art and sound assets, and third-party licences are listed

#### Scenario: Notices in the web build
- **WHEN** the published web build is inspected
- **THEN** it contains the code and asset licence notices
