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
