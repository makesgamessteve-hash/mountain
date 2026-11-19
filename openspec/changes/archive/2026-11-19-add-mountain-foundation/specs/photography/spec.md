# photography Specification Delta

## Purpose

Lets the player compose and take photos of the views in a dedicated camera mode with zoom and filters, keeps them as image files in the user data folder, and lets the player browse, delete and (on the web) download them.

## ADDED Requirements

### Requirement: Camera mode
Pressing the camera input (C, gamepad RB or the touch camera button) SHALL enter camera mode, which hides the HUD and the walking touch controls and shows only a minimal viewfinder (thirds grid, vignette, zoom and filter) with camera buttons on touch screens. Pressing the camera input again, Esc or gamepad B MUST leave camera mode. The player can keep moving and looking in camera mode.

#### Scenario: Entering camera mode
- **WHEN** the player presses C during play
- **THEN** the HUD disappears and a minimal viewfinder is shown

#### Scenario: Leaving camera mode
- **GIVEN** camera mode is active
- **WHEN** the player presses C again
- **THEN** camera mode ends and the HUD is shown again

### Requirement: Zoom
In camera mode the player SHALL be able to zoom by changing the field of view between 20 and 90 degrees, using the mouse wheel, the + and - keys, gamepad D-pad up and down, or zoom buttons on touch screens. The field of view MUST stay within these limits.

#### Scenario: Zooming in to the limit
- **GIVEN** camera mode is active
- **WHEN** the player keeps zooming in
- **THEN** the field of view narrows and stops at 20 degrees

#### Scenario: Zooming out to the limit
- **GIVEN** camera mode is active
- **WHEN** the player keeps zooming out
- **THEN** the field of view widens and stops at 90 degrees

### Requirement: Photo filters
In camera mode the player SHALL be able to cycle through the filters none, sepia, black and white, vivid and dusk (F key, gamepad D-pad left and right, or the touch filter button). The viewfinder MUST preview the selected filter live and the saved photo uses it.

#### Scenario: Sepia photo
- **GIVEN** camera mode is active with the sepia filter selected
- **WHEN** a photo is taken
- **THEN** the viewfinder showed sepia tones and the saved photo is in sepia tones

#### Scenario: Black and white
- **WHEN** a photo is taken with the black and white filter
- **THEN** every pixel of the saved photo has equal red, green and blue values

### Requirement: Taking a photo
In camera mode, pressing the shutter input (left mouse button or Enter, gamepad RT, or the touch shutter button) SHALL take a photo with a shutter sound and a brief white flash on screen. The photo MUST contain the rendered scene with the selected filter only, without HUD, viewfinder, flash, messages or touch controls.

#### Scenario: Shooting
- **GIVEN** camera mode is active
- **WHEN** the player clicks the left mouse button
- **THEN** a shutter sound plays, the screen flashes briefly and a new photo is saved
- **AND** the saved photo shows no user-interface elements

### Requirement: Photo files
Each photo SHALL be saved as a PNG file in a `photos` folder inside the user data directory, named with the world seed and the local date and time of shooting (for example `mountain-12345-20261119-143205.png`). The image MUST be the low-resolution retro frame enlarged by the smallest whole-number factor that makes it at least 720 pixels tall, with crisp, unblurred pixels.

#### Scenario: Image size at the default resolution
- **GIVEN** the internal resolution is 216 lines
- **WHEN** a photo is taken
- **THEN** the saved PNG is 864 pixels tall and each retro pixel is a sharp 4 by 4 block

#### Scenario: Image size at 360 lines
- **GIVEN** the internal resolution is 360 lines
- **WHEN** a photo is taken
- **THEN** the saved PNG is 720 pixels tall

#### Scenario: File name
- **GIVEN** the world seed is 12345
- **WHEN** a photo is taken on 19 November 2026 at 14:32:05 local time
- **THEN** the file name contains 12345 and 20261119-143205 and ends with .png

#### Scenario: Two photos in the same second
- **WHEN** two photos are taken within the same second
- **THEN** both are saved under different file names

### Requirement: In-game gallery
Pressing the gallery input (G, gamepad Back/View or the touch gallery button) SHALL open a gallery that lists all saved photos as thumbnails, newest first. The player MUST be able to view any photo large, delete it after a confirmation, and close the gallery to return to play.

#### Scenario: Browsing photos
- **GIVEN** 5 photos have been taken
- **WHEN** the player presses G
- **THEN** the gallery shows 5 thumbnails with the newest first

#### Scenario: Deleting a photo
- **GIVEN** the gallery is open showing a selected photo
- **WHEN** the player chooses delete and confirms
- **THEN** the photo file is removed and its thumbnail disappears from the gallery

#### Scenario: Empty gallery
- **GIVEN** no photos have been taken
- **WHEN** the player opens the gallery
- **THEN** a friendly message explains how to take photos

### Requirement: Photo folder on desktop
On desktop builds the gallery SHALL show the full path of the folder in which photos are saved, so the player can find them outside the game.

#### Scenario: Folder path shown
- **WHEN** the gallery is opened on Windows or Linux
- **THEN** the full path of the photos folder is displayed

### Requirement: Downloading photos on the web
In the web build every photo in the gallery SHALL offer a download that saves the PNG through the browser under its photo file name. Right after a photo is taken, a download button for that photo MUST be offered for at least 5 seconds.

#### Scenario: Download after shooting
- **GIVEN** the web build
- **WHEN** a photo is taken
- **THEN** a download button for that photo appears for at least 5 seconds
- **AND** pressing it makes the browser save the PNG file

#### Scenario: Download from the gallery
- **GIVEN** the web build and a gallery with photos
- **WHEN** the player chooses download on a photo
- **THEN** the browser saves that photo's PNG file
