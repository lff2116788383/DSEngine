#pragma once

// Font Awesome 6 Free Solid - 16-bit PUA codepoints (no WCHAR32 needed)
// Font: https://github.com/FortAwesome/Font-Awesome (fa-solid-900.ttf)
// Header ref: https://github.com/juliettef/IconFontCppHeaders

#define FA_ICON_MIN 0xF000
#define FA_ICON_MAX 0xF8FF

// Toolbar icons
#define MDI_ICON_CURSOR_DEFAULT_OUTLINE "\xef\x89\x85" // U+F245 fa-arrow-pointer
#define MDI_ICON_ARROW_ALL              "\xef\x82\xb2" // U+F0B2 fa-arrows-up-down-left-right
#define MDI_ICON_ROTATE_3D_VARIANT      "\xef\x80\x9e" // U+F01E fa-arrow-rotate-right
#define MDI_ICON_RESIZE                 "\xef\x81\xa4" // U+F064 fa-up-right-and-down-left-from-center (approx: fa-expand)
#define MDI_ICON_PLAY                   "\xef\x81\x8b" // U+F04B fa-play
#define MDI_ICON_STOP                   "\xef\x81\x8d" // U+F04D fa-stop
#define MDI_ICON_PAUSE                  "\xef\x81\x8c" // U+F04C fa-pause
#define MDI_ICON_SKIP_NEXT              "\xef\x81\x91" // U+F051 fa-forward-step
#define MDI_ICON_ARROW_DOWN             "\xef\x81\xa3" // U+F063 fa-arrow-down
#define MDI_ICON_ARROW_UP               "\xef\x81\xa2" // U+F062 fa-arrow-up

// Hierarchy entity type icons
#define MDI_ICON_CUBE_OUTLINE           "\xef\x86\xb2" // U+F1B2 fa-cube
#define MDI_ICON_CAMERA                 "\xef\x80\xb0" // U+F030 fa-camera
#define MDI_ICON_VIDEO                  "\xef\x80\xb0" // U+F030 fa-camera (same)
#define MDI_ICON_LIGHTBULB              "\xef\x83\xab" // U+F0EB fa-lightbulb
#define MDI_ICON_IMAGE                  "\xef\x80\xbe" // U+F03E fa-image
#define MDI_ICON_SHAPE                  "\xef\x97\x9e" // U+F5DE fa-shapes
#define MDI_ICON_ATOM                   "\xef\x97\x91" // U+F5D1 fa-atom (approx, might not exist in solid)
#define MDI_ICON_ANIMATION              "\xef\x80\x88" // U+F008 fa-film
#define MDI_ICON_TERRAIN                "\xef\x89\xb9" // U+F279 fa-map (fa-mountain-sun is FA6 Pro only)
#define MDI_ICON_EYE                    "\xef\x81\xae" // U+F06E fa-eye
#define MDI_ICON_COLLIDE                "\xef\x84\x9c" // U+F11C fa-keyboard (approx for collider)
#define MDI_ICON_WEATHER_SUNNY          "\xef\x86\x85" // U+F185 fa-sun
#define MDI_ICON_MOON                   "\xef\x86\x86" // U+F186 fa-moon
#define MDI_ICON_VIEW_IN_AR             "\xef\x86\xb2" // U+F1B2 fa-cube
#define MDI_ICON_LABEL                  "\xef\x80\xab" // U+F02B fa-tag
#define MDI_ICON_COG                    "\xef\x80\x93" // U+F013 fa-gear
#define MDI_ICON_FLASK                  "\xef\x83\x83" // U+F0C3 fa-flask
#define MDI_ICON_PACKAGE_VARIANT        "\xef\x81\xbb" // U+F07B fa-folder
#define MDI_ICON_FOLDER                 "\xef\x81\xbb" // U+F07B fa-folder

// Inspector component icons
#define MDI_ICON_AXIS_ARROW             "\xef\x82\xb2" // U+F0B2 fa-arrows - Transform
#define MDI_ICON_PALETTE                "\xef\x94\xbf" // U+F53F fa-palette - Sprite
#define MDI_ICON_RUN                    "\xef\x9c\x8c" // U+F70C fa-person-running - RigidBody
#define MDI_ICON_SPHERE                 "\xef\x86\xb2" // U+F1B2 fa-cube - Mesh
#define MDI_ICON_CREATION               "\xef\x97\x91" // U+F5D1 fa-atom - Particle
#define MDI_ICON_FORMAT_TEXT            "\xef\x80\xb1" // U+F031 fa-font - UILabel
#define MDI_ICON_POST_PROCESS           "\xef\x94\xab" // U+F52B fa-wand-magic-sparkles - PostProcess
#define MDI_ICON_FILE                   "\xef\x85\x9b" // U+F15B fa-file - Script
#define MDI_ICON_BUTTON                 "\xef\x82\x96" // U+F096 fa-square (outline) - UIButton
#define MDI_ICON_CIRCLE                 "\xef\x84\x91" // U+F111 fa-circle - CircleCollider

// Console panel icons
#define MDI_ICON_INFORMATION            "\xef\x81\x9a" // U+F05A fa-circle-info
#define MDI_ICON_ALERT                  "\xef\x81\xb1" // U+F071 fa-triangle-exclamation
#define MDI_ICON_CLOSE_CIRCLE           "\xef\x81\x97" // U+F057 fa-circle-xmark

// Hierarchy search
#define MDI_ICON_MAGNIFY                "\xef\x80\x82" // U+F002 fa-magnifying-glass

// Lua Console
#define MDI_ICON_CODE                   "\xef\x84\xa1" // U+F121 fa-code

// Menu bar icons
#define MDI_ICON_PLUS                   "\xef\x81\xa7" // U+F067 fa-plus
#define MDI_ICON_FOLDER_OPEN            "\xef\x81\xbc" // U+F07C fa-folder-open
#define MDI_ICON_CONTENT_SAVE           "\xef\x83\x87" // U+F0C7 fa-floppy-disk
#define MDI_ICON_EXPORT                 "\xef\x95\xae" // U+F56E fa-file-export
#define MDI_ICON_PUZZLE                 "\xef\x84\xae" // U+F12E fa-puzzle-piece

// Project panel view toggles
#define MDI_ICON_VIEW_LIST              "\xef\x80\xba" // U+F03A fa-list
#define MDI_ICON_VIEW_GRID              "\xef\x83\x8e" // U+F0CE fa-table

// Project panel file-type icons
#define MDI_ICON_FILE_OUTLINE           "\xef\x85\x9b" // U+F15B fa-file
#define MDI_ICON_SCRIPT_TEXT_OUTLINE    "\xef\x87\x89" // U+F1C9 fa-file-code
#define MDI_ICON_IMAGE_MULTIPLE         "\xef\x8c\x82" // U+F302 fa-images
#define MDI_ICON_HUMAN                  "\xef\x80\x87" // U+F007 fa-user
#define MDI_ICON_MUSIC_NOTE             "\xef\x80\x81" // U+F001 fa-music

// Additional icons for new panels
#define MDI_ICON_VOLUME_HIGH            "\xef\x80\xa8" // U+F028 fa-volume-high
#define MDI_ICON_BONE                   "\xef\x97\x97" // U+F5D7 fa-bone
#define MDI_ICON_CONTENT_COPY           "\xef\x83\x85" // U+F0C5 fa-copy
#define MDI_ICON_ZIP_BOX                "\xef\x86\x86" // U+F186 fa-file-zipper (approx)
#define MDI_ICON_VIEW_MODULE            "\xef\x80\x9a" // U+F00A fa-table-cells
#define MDI_ICON_HOME                   "\xef\x80\x95" // U+F015 fa-house
#define MDI_ICON_HELP                   "\xef\x81\x99" // U+F059 fa-circle-question
#define MDI_ICON_DELETE                 "\xef\x87\xb8" // U+F1F8 fa-trash-can
#define MDI_ICON_SOURCE_BRANCH          "\xef\x84\xa6" // U+F126 fa-code-branch
#define MDI_ICON_MAP_MARKER_PATH        "\xef\x97\xa6" // U+F5E6 fa-route
#define MDI_ICON_SPOTLIGHT_BEAM         "\xef\x83\xab" // U+F0EB fa-lightbulb (reuse)
#define MDI_ICON_WHITE_BALANCE_SUNNY    "\xef\x86\x85" // U+F185 fa-sun
#define MDI_ICON_SKIP_PREVIOUS          "\xef\x81\x88" // U+F048 fa-backward-step
#define MDI_ICON_CLOUD_DOWNLOAD         "\xef\x83\xad" // U+F0ED fa-cloud-arrow-down
#define MDI_ICON_CHART_LINE             "\xef\x88\x81" // U+F201 fa-chart-line
#define MDI_ICON_SITEMAP                "\xef\x83\xa8" // U+F0E8 fa-sitemap
#define MDI_ICON_WATER                  "\xef\x9d\xb3" // U+F773 fa-water
#define MDI_ICON_MAP                    "\xef\x89\xb9" // U+F279 fa-map
#define MDI_ICON_LAYERS                 "\xef\x97\xbd" // U+F5FD fa-layer-group

// Editor feature panel icons
#define MDI_ICON_BUG                    "\xef\x86\x88" // U+F188 fa-bug
#define MDI_ICON_BRUSH                  "\xef\x87\xbc" // U+F1FC fa-paintbrush
#define MDI_ICON_CHECK                  "\xef\x80\x8c" // U+F00C fa-check
#define MDI_ICON_CIRCLE_MEDIUM          "\xef\x84\x91" // U+F111 fa-circle
#define MDI_ICON_CIRCLE_OUTLINE         "\xef\x84\x91" // U+F111 fa-circle (same glyph)
#define MDI_ICON_CIRCLE_SLICE_8         "\xef\x84\x91" // U+F111 fa-circle (approx)
#define MDI_ICON_CLOUD                  "\xef\x83\x82" // U+F0C2 fa-cloud
#define MDI_ICON_CLOUD_UPLOAD           "\xef\x83\xae" // U+F0EE fa-cloud-arrow-up
#define MDI_ICON_CONSOLE                "\xef\x84\xa0" // U+F120 fa-terminal
#define MDI_ICON_DEBUG_STEP_INTO        "\xef\x81\xa3" // U+F063 fa-arrow-down (approx)
#define MDI_ICON_DEBUG_STEP_OVER        "\xef\x81\xa1" // U+F061 fa-arrow-right (approx)
#define MDI_ICON_FILE_ALERT             "\xef\x85\x9b" // U+F15B fa-file (approx)
#define MDI_ICON_FILE_DOCUMENT_EDIT     "\xef\x85\x9c" // U+F15C fa-file-lines
#define MDI_ICON_FLASH                  "\xef\x83\xa7" // U+F0E7 fa-bolt
#define MDI_ICON_FORMAT_LIST_NUMBERED   "\xef\x83\xa2" // U+F0E2 fa-list-ol (approx)
#define MDI_ICON_GRADIENT_HORIZONTAL    "\xef\x94\xbf" // U+F53F fa-palette (approx)
#define MDI_ICON_GRID                   "\xef\x83\x8e" // U+F0CE fa-table
#define MDI_ICON_HISTORY                "\xef\x87\x9a" // U+F1DA fa-clock-rotate-left
#define MDI_ICON_MOVIE                  "\xef\x80\x88" // U+F008 fa-film
#define MDI_ICON_MOVIE_OPEN             "\xef\x80\x88" // U+F008 fa-film (same)
#define MDI_ICON_REFRESH                "\xef\x80\xa1" // U+F021 fa-arrows-rotate
#define MDI_ICON_RELOAD                 "\xef\x80\x9e" // U+F01E fa-arrow-rotate-right
#define MDI_ICON_RELOAD_ALERT           "\xef\x80\x9e" // U+F01E fa-arrow-rotate-right (same)
#define MDI_ICON_TRANSIT_CONNECTION_VARIANT "\xef\x84\xa6" // U+F126 fa-code-branch (approx)
#define MDI_ICON_VARIABLE               "\xef\x84\xa1" // U+F121 fa-code (approx)
