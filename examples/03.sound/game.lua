
--[[
   This is a simple example that plays a short music track.

   You can copy, alter or reuse this file any way you like.
   It is available without restrictions (public domain).
]]

print("Sound example in blitwizard")


function blitwiz.on_init()
	-- Open a window
	blitwiz.graphics.setWindow(640,480,"Sound", false) -- resolution/size: 640x480, title: "Hello World", fullscreen: false/no

	-- Load image
	blitwiz.graphics.loadImage("background.png")

	-- Play song
	blitwiz.sound.play("blitwizarddemosongloop.ogg", 1.0, -0.2, true) -- play sound "blitwizarddemosongloop.ogg" with volume 1.0 (full), panning -0.2 (slightly left) and repeat true (enabled, so will repeat forever)
end

function blitwiz.on_keydown(key)
	-- When escape is pressed, we want to quit
    if key == "escape" then
		os.exit(0)
	end
end

function blitwiz.on_draw()
	-- When the image is loaded, draw it centered:
	local w,h = blitwiz.graphics.getImageSize("background.png")
	local mw,mh = blitwiz.graphics.getWindowSize()

	-- Actual drawing happens here
	blitwiz.graphics.drawImage("background.png", mw/2 - w/2, mh/2 - h/2)

	-- Done!
end

function blitwiz.on_close()
	-- This function gets called whenever the user clicks
	-- the close button in the window title bar.
	
	-- The user has attempted to close the window,
	-- so we want to respect his wishes and quit :-)
	os.exit(0)
end


