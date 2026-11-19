# platform-targets Specification Delta

## Purpose

Defines where the game runs and how it adapts to each platform: native Windows and Linux builds, a browser build for desktop and mobile with automatic touch controls, an immersive WebXR VR mode, and where per-user settings and photos are kept.

## ADDED Requirements

### Requirement: Native desktop builds
The game SHALL be delivered as native 64-bit executables for Windows (x64) and Linux (x64) that start in a resizable window, adapt the rendering to the window size and toggle fullscreen with F11.

#### Scenario: Starting on Linux
- **GIVEN** the Linux build with its asset folders beside the executable
- **WHEN** the executable is started
- **THEN** a window opens showing the game

#### Scenario: Fullscreen toggle
- **WHEN** the player presses F11
- **THEN** the game switches between windowed and fullscreen display

### Requirement: Web build
The game SHALL be delivered as a web build (WebAssembly with WebGL2) that runs in the current desktop and mobile versions of Chrome, Edge, Firefox and Safari, fills the browser viewport, follows window resizing and device rotation, and shows loading progress while downloading. Browsers without WebGL2 MUST get a friendly explanation instead of a blank page.

#### Scenario: Playing in a browser
- **WHEN** the web build's page is opened in a current desktop browser
- **THEN** a loading progress indicator is shown and then the game runs in the page

#### Scenario: Rotating a phone
- **GIVEN** the web build running on a phone in portrait orientation
- **WHEN** the phone is rotated to landscape
- **THEN** the game fills the new viewport

#### Scenario: No WebGL2
- **WHEN** the page is opened in a browser without WebGL2 support
- **THEN** a message explains that a browser with WebGL2 is required

### Requirement: Persistent settings and user data
Settings (resolution, volume, time speed and the last world seed) and photos SHALL be kept in a per-user data location: the operating system's per-user application data folder on desktop, and the browser's persistent storage on the web where the browser allows it. Settings MUST be restored at the next start.

#### Scenario: Settings survive a restart
- **GIVEN** the player set the master volume to 40 percent
- **WHEN** the game is closed and started again
- **THEN** the master volume is 40 percent

#### Scenario: Photos survive a reload on the web
- **GIVEN** the web build in a browser that allows persistent storage and 2 photos taken
- **WHEN** the page is reloaded
- **THEN** the gallery still shows the 2 photos

### Requirement: Automatic touch controls
The web build SHALL show touch controls when the device's primary input is touch or as soon as a touch event occurs, and hide them again as soon as real keyboard or mouse input occurs.

#### Scenario: Phone starts with touch controls
- **WHEN** the web build is opened on a phone
- **THEN** the touch controls are visible from the start

#### Scenario: First touch on a laptop
- **GIVEN** the web build on a touch-screen laptop showing keyboard and mouse controls
- **WHEN** the player touches the screen
- **THEN** the touch controls appear

#### Scenario: Back to keyboard
- **GIVEN** touch controls are visible
- **WHEN** the player presses a key on a physical keyboard or moves a physical mouse
- **THEN** the touch controls are hidden

### Requirement: Touch control layout
Touch controls SHALL consist of a virtual joystick in the bottom-left area for moving (pushing it fully runs), dragging on the right half of the screen to look, and on-screen buttons for interact, jump, house, camera, gallery and menu. Moving and looking MUST work at the same time with two fingers; in camera mode, shutter, filter and zoom buttons are offered.

#### Scenario: Move and look together
- **GIVEN** touch controls are visible
- **WHEN** one finger pushes the joystick forward while another drags to the right on the right half
- **THEN** the player walks forward while turning right

#### Scenario: Buttons
- **GIVEN** touch controls are visible
- **WHEN** the player taps the house button
- **THEN** the house is summoned as with the H key

### Requirement: Entering VR
When the browser supports immersive virtual reality, the web build SHALL offer an "Enter VR" button; otherwise no such button is shown. Entering VR MUST render the scene in stereo for the headset with head tracking, keeping the retro look.

#### Scenario: VR offered
- **GIVEN** a browser and headset that support immersive VR
- **WHEN** the web build is loaded
- **THEN** an "Enter VR" button is shown
- **AND** pressing it starts a stereo VR view that follows the head movement

#### Scenario: VR not available
- **WHEN** the web build is loaded in a browser without immersive VR support
- **THEN** no "Enter VR" button is shown

### Requirement: VR controls
In VR the left thumbstick SHALL move the player relative to the viewing direction, the right thumbstick snap-turns by 30 degrees per flick, the trigger interacts and a face button takes a photo of the current view.

#### Scenario: Snap turn
- **GIVEN** an active VR session
- **WHEN** the right thumbstick is flicked to the right once
- **THEN** the view turns right by exactly 30 degrees in one step

#### Scenario: Photo in VR
- **GIVEN** an active VR session
- **WHEN** the player presses the photo face button
- **THEN** a photo of the current view is saved as described for photos

#### Scenario: Opening the door in VR
- **GIVEN** an active VR session and the player looking at the house door from 2 m
- **WHEN** the player pulls the trigger
- **THEN** the door opens

### Requirement: Leaving VR
Leaving VR, from the game or through the browser or headset, SHALL return the web build to its normal view at the player's current position without reloading.

#### Scenario: Exit VR
- **GIVEN** an active VR session with the player at the summit
- **WHEN** the VR session ends
- **THEN** the normal view is shown with the player still at the summit
