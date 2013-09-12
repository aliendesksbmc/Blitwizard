
--[[
   This is a simple example that simply loads an image
   and then displays it to the user in a 640x480 window.

   Moreover, the user can close the window.

   You can copy, alter or reuse this file any way you like.
   It is available without restrictions (public domain).
]]

print("Hello world example in blitwizard")

-- All blitwizard.on* functions (e.g. blitwizard.onInit)
-- are predetermined by blitwizard and are called when
-- various things happen (if you have added them to
-- your program).
-- This allows you to react to certain events
-- (e.g. program has loaded, the window is redrawn etc.).

-- For more information on which callback functions are
-- available, check the other examples or see the full
-- documentation at http://www.blitwizard.de/doc-files/api-stable

-- Warn if we run without templates (you can remove this from
-- your own game if you wish to, it is just for convenience)
if blitwizard.templatesinitialised ~= true then
    error "The templates/ sub folder with the templates is apparently missing. Please copy it into the same folder as your game.lua before you start up."
end

function blitwizard.onInit()
	-- This function is called right after blitwizard has
	-- started.
	-- You may want to open up a window here,
	-- using blitwizard.graphics.setMode().

	-- Open a window
    blitwizard.graphics.setMode(640, 480, "Hello World", false)

    -- Create hello world:
    local helloworld = blitwizard.object:new(
    blitwizard.object.o2d, "hello_world.png")
    helloworld:setPosition(0, -2)

    -- Create system info title:
    local si = blitwizard.object:new(
    blitwizard.object.o2d, "system_info.png")
    si:pinToCamera()

    -- Create system info text:
    local stxt = blitwizard.font.text:new(
    _VERSION .. ",\nrunning on: "  ..
    os.sysname() .. " (" .. os.sysversion() ..
    ")")

    -- As soon as the system info image (si) is loaded,
    -- we would like to place it accordingly and move
    -- the text aswell.
    -- It needs to be loaded so we know its dimensions.
    function si:onGeometryLoaded()
        -- place at left bottom:
        local camw,camh =
        blitwizard.graphics.getCameras()[1]:getDimensions()

        local imgw,imgh =
        self:getDimensions()
        
        self:setPosition(1, camh - imgh - 1)

        -- move text aswell:
        local mx,my = self:getPosition()
        stxt:setPosition(mx + 0.5, my + 1)
    end
end

function blitwizard.onKeyDown(key)
	-- When pressing space, we can switch between
	-- accelerated and software rendering with this:
	if key == "space" then
		if switchedtosoftware ~= true then
			blitwizard.graphics.setMode(600, 480,
            "Hello World", false, "software")
			switchedtosoftware = true
			print("Now: software mode")
		else
			blitwizard.graphics.setMode(600, 480,
            "Hello World", false)
			switchedtosoftware = false
			print("Now: accelerated mode")
		end
	end
	-- When escape is pressed, we want to quit
    if key == "escape" then
		os.exit(0)
	end
end

function blitwizard.onClose()
	-- This function gets called whenever the user clicks
	-- the close button in the window title bar.
	
	-- The user has attempted to close the window,
	-- so we want to respect his wishes and quit :-)
	os.exit(0)
end



