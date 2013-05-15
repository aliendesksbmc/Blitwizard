--[[
blitwizard.font
Under the zlib license:

Copyright (c) 2012-2013 Jonas Thiem

This software is provided 'as-is', without any express or implied warranty. In no event will the authors be held liable for any damages arising from the use of this software.

Permission is granted to anyone to use this software for any purpose, including commercial applications, and to alter it and redistribute it freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not claim that you wrote the original software. If you use this software in a product, an acknowledgment in the product documentation would be appreciated but is not required. 

2. Altered source versions must be plainly marked as such, and must not be misrepresented as being the original software.

3. This notice may not be removed or altered from any source distribution.

]]

--[[--
 The font namespace allows to draw bitmap fonts.
 One bitmap font is included and can be used by submitting "default".

 <i>Keep in mind when releasing your game:</i> please note this module
 is implemented in the
 <b>blitwizard templates</b>, so it is unavailable if you don't ship
 the templates with your game.
 @author Jonas Thiem (jonas.thiem@gmail.com)
 @copyright 2013
 @license zlib
 @module blitwizard.font
]]

blitwizard.font = {}
blitwizard.font.text = {}

--[[--
  The text object represents a rendered text. Create one using
  @{blitwizard.font.text:new}. You can destroy it again
  by using @{blitwizard.font.text:destroy}.

  @type text
  @usage
  -- Create a text saying "Hello World" in the upper left corner
  -- (with a scaling of 2, so twice the normal font size)
  local myText = blitwizard.font.text:new("Hello World",
    "default", 2)

  -- Move it a bit away from the upper left corner:
  myText:setPosition(0.6, 0.6)
]]

--[[--
  Create a text object. You submit a string, a font bitmap path
  (can be "default" for standard font), and various layout
  information like maximum width of the text block in game units
  (will result in line wraps if text is too long) and font scaling.

  @function new
  @tparam string text the text you want to display
  @tparam string (optional) font the path to the bitmap of the bitmap font you want to use. You can also specify nil or "default" for the default font
  @tparam number (optional) scale the scale of the font. 1 for normal scale, anything else for scaling up (larger)/down (smaller). How large that turns out depends on how large the font originally was - give it a try!
  @tparam userdata camera the game camera to show the text on. Specify nil for the default camera (first one from @{blitwizard.graphics.getCameras})
  @tparam number width (optional, pass nil if you don't want to specify) the intended maximum width of the text in game units. If the rendered text was to exceed that width, line wraps will be applied to prevent that - it turns into a multi line text with multiple lines.
  @tparam number glyphWidth width of a glyph in the bitmap font in pixels. <b>Optional for the "default" font.</b>
  @tparam number glyphHeight height of a glyph in the bitmap font in pixels. <b>Optional for the "default" font.</b>
  @tparam number glyphsPerLine the amount of glyphs per line, if not texture width divided by glyphWidth. <b>Optional for the "default" font.</b>
  @treturn table returns @{blitwizard.font.text|text object}
]]

function blitwizard.font.text:new(text, fontPath, scale, camera, width,
glyphWidth, glyphHeight, glyphsPerLine)
    -- parameter validation:
    if type(text) ~= "string" then
        error("bad parameter #1 to blitwizard.font.text:create: expected " ..
        "string")
    end
    if type(fontPath) ~= "string" then
        if fontPath == nil then
            fontPath = "default"
        else
            error("bad parameter #2 to blitwizard.font.text:create: " ..
            "expected string")
        end
    end
    if type(scale) ~= "number" then
        if scale == nil then
            scale = 1
        else
            error("bad parameter #3 to blitwizard.font.text:create: " ..
            "expected number")
        end
    end
    if type(width) ~= "number" and type(width) ~= "nil" then
        error("bad parameter #4 to blitwizard.font.text:create: expected " ..
        "number or nil")
    end
    if fontPath ~= "default" or type(glyphWidth) ~= "nil"
    or type(glyphHeight) ~= "nil" or type(glyphsPerLine) ~= "nil" then
        if type(glyphWidth) ~= "number" then
            error("bad parameter #5 to blitwizard.font.text:create: " ..
            "expected number")
        end
        if type(glyphHeight) ~= "number" then
            error("bad parameter #6 to blitwizard.font.text:create: " ..
            "expected number")
        end
        if type(glyphsPerLine) ~= "number" then
            error("bad parameter #7 to blitwizard.font.text:create: " ..
            "expected number")
        end
    end

    if camera == nil then
        camera = blitwizard.graphics.getCameras()[1]
    end

    local t = {
        font = fontPath,
        glyphs = {},
        _width = 0,
        _height = 0,
        _x = 0,
        _y = 0,
    }
    local mt = {
        __index = blitwizard.font.text
    }
    setmetatable(t, mt)

    if fontPath == "default" then
        -- use default settings:
        fontPath = os.templatedir() .. "/font/default.png"
        glyphWidth = 7
        glyphHeight = 14
        glyphsPerLine = 32
    end

    -- obvious early abort conditions:
    if #text <= 0 or (width ~= nil and (width or 0) <= 0)
    or glyphWidth <= 0 or glyphHeight <= 0 or scale <= 0
    or (glyphsPerLine ~= nil and (glyphsPerLine or 0) <= 0) then
        return t
    end

    local charsPerLine = #text
    if type(width) ~= "nil" or (width or 0) < 0 then
        -- calculate horizontal char limit:
        charsPerLine = math.floor(width / glyphWidth)
    end

    -- see if we need to insert line breaks:


    -- now create all glyph objects:
    local i = 1
    local x = 0
    local y = 0
    local gameUnitsInPixels =
    blitwizard.graphics.getCameras()[1]:gameUnitToPixels()
    local glyphXShift = glyphWidth / gameUnitsInPixels
    local glyphYShift = glyphHeight / gameUnitsInPixels
    t._glyphDimensionX = glyphXShift
    t._glyphDimensionY = glyphYShift
    -- walk through all glyphs:
    while i <= #text do
        local character = string.sub(text, i, i)
        if character == "\r" or character == "\n" then
            -- line break!
            x = -glyphXShift
            y = y + glyphYShift
            -- handle \r\n properly:
            if i < #text and character == "\r" then
                if string.sub(text, i + 1, i + 1) == "\n" then
                    i = i + 1
                end
            end
        else
            -- get letter font pos:
            local charxslot = string.byte(character)-32+1
            local charyslot = 1
            while charxslot > glyphsPerLine do
                charxslot = charxslot - glyphsPerLine
                charyslot = charyslot + 1 
            end

            -- add new glyph:
            local newGlyph = blitwizard.object:new(
                blitwizard.object.o2d, fontPath)
            newGlyph.offset = {x = x * scale, y = y * scale}
            newGlyph:setPosition(newGlyph.offset.x, newGlyph.offset.y)
            newGlyph:pinToCamera(camera)
            newGlyph:setScale(scale, scale)
            newGlyph:set2dTextureClipping((charxslot - 1) * glyphWidth, 
            (charyslot - 1) * glyphHeight, glyphWidth, glyphHeight)
            t.glyphs[#t.glyphs+1] = newGlyph

            -- update overall text block width and height:
            if t._width < x + glyphWidth then
                t._width = x + glyphWidth / gameUnitsInPixels
            end
            if t._height < y + glyphHeight then
                t._height = y + glyphHeight / gameUnitsInPixels
            end
        end
        i = i + 1
        x = x + glyphWidth / gameUnitsInPixels
    end

    -- return text:
    return t
end

--[[--
  Destroy a @{blitwizard.font.text|text} object again.
  (If you don't do this, it will stay on the screen forever!)
  @function destroy
]]
function blitwizard.font.text:destroy()
    for i,v in ipairs(self.glyphs) do
        -- destroy all glyphs:
        v:destroy()
    end
end


--[[--
  Move the text object to the given screen position in game units
  (0,0 is top left).
  @function move
  @tparam number x new x position in game units (0 is left screen border)
  @tparam number y new y position in game units (0 is top screen border)
]]
function blitwizard.font.text:move(x, y)
    self._x = x
    self._y = y
    for i,v in ipairs(self.glyphs) do
        -- move all glyphs:
        v:setPosition(v.offset.x + x, v.offset.y + y)
    end
end

--[[--
  Get the current top-left position of the text object.
  @function getPosition
  @treturn number x coordinate
  @treturn number y coordinate
]]
function blitwizard.font.text:getPosition()
    return self._x, self._y
end

--[[--
  Get the size of a glyph in game units.
  @function getGlyphDimensions
  @treturn number x dimensions of glyph (in game units!)
  @treturn number y dimensions of glyph (in game units!)
]]
function blitwizard.font.text:getGlyphDimensions()
    return self._glyphDimensionX, self._glyphDimensionY
end

--[[--
  Set the Z index of the text block (see @{blitwizard.object:setZIndex}).
  @function setZIndex
  @tparam number zindex Z index
]]
function blitwizard.font.text:setZIndex(index)
    for i,v in ipairs(self.glyphs) do
        v:setZIndex(index)
    end
end

--[[--
  Set the visibility of a text block. This allows you to temporarily hide
  a text object (see @{blitwizard.object:setVisible}).
  @function setVisible
  @tparam boolean visibility true if visible, false if not
]]
function blitwizard.font.text:setVisible(visible)
    for i,v in ipairs(self.glyphs) do
        v:setVisible(visible)
    end
end

--[[--
  The width in game units of the complete text block.
  @function width
]]
function blitwizard.font.text:width()
    return self._width
end

--[[--
  The height of te text block in game units.
  @function height
]]
function blitwizard.font.text:height()
    return self._height
end

--[[
    -- calculate wrap width:
    local maxperline = nil
    if wrapwidth ~= nil then
        maxperline = math.floor(wrapwidth/font[2])
        if maxperline < 1 then
            maxperline = 1
        end
    end
    -- draw the font char by char
    local i = 1
    local charsperline = 0
    while i <= #text do
        local character = string.byte(string.sub(text, i, i))
        if character == string.byte("\n") then
            -- user defined line breaks should be possible:
            posx = origposx
            posy = posy + font[3]
            charsperline = 0
        else 
            -- adhere to maximum line length
            local linefull = false
            if maxperline then
                if charsperline >= maxperline then
                    charsperline = -1
                    linefull = true
                    posx = origposx
                    posy = posy + font[3]
                    i = i - 1 -- revoke later skipping of current char
                end
            end
            -- draw char:
            if not linefull then
                local slot = (character - string.byte(" "))+1
                drawfontslot(font, slot, posx, posy, r, g, b, a, clipx, clipy, clipw, cliph)
                posx = posx + font[2]
            end
        end
        i = i + 1
        charsperline = charsperline + 1
    end
end

function blitwiz.font.addToString_inner(str, line, maxlinelength, maxlinecount)
    -- Split line when horizontally too long
    while #line > maxlinelen do
        str = str .. string.sub(line, 1, maxlinelen) .. "\n"
        line = "  " .. string.sub(line, maxlinelen + 1)
    end
    str = str .. line .. "\n"

    -- Scroll text when too many vertical lines
    local linecount = #{string.split(str, "\n")}
    while linecount > maxlines do
        local throwaway = ""
        throwaway,str = string.split(str, "\n", 1)
        linecount = linecount - 1
    end
    return str
end

function blitwiz.font.addToString(str, line, maxlinelength, maxlinecount)
    local lines = {string.split(line, "\n")}
    for key,value in ipairs(lines) do
        str = blitwiz.font.addToString_inner(str, value, maxlinelength, maxlinecount)
    end
    return str
end

blitwiz.font.register("font/default.png", "default", 7, 14, 32, "iso-8859-15")
]]

