-- config.ld for wxLanesBridge
project = "wxLanesBridge"
title = "wxLanesBridge Documentation"
-- description = "A bridge module for safe C++ pointer exchange between wxLua and Lua Lanes"
-- full_description = [[
-- wxLanesBridge provides a set of C++ functions to facilitate communication 
-- between different Lua Lanes and the wxWidgets GUI thread. 
-- 
-- Main features:
-- - Extracting raw pointers from wxLua objects.
-- - Triggering the GUI event loop from worker threads (Doorbell pattern).
-- - Lane-safe pointer passing using lightuserdata.
-- ]]

-- Input and Output
file = { "src/wxLanesBridge.cpp" } -- Add other .cpp or .lua files here
dir = "doc"             -- The output directory for HTML files

-- Technical Settings
-- language = c          -- Required to parse C++ files
not_luadoc = true
format = "markdown"     -- Allows using Markdown in your comments
wrap = true             -- Wrap long lines in the documentation

-- Styling
no_summary = false      -- Show summaries on the module page
no_return_or_parms = false

-- Interface
-- This allows linking to external Lua documentation if needed
-- examples = "examples" -- If you have an examples folder
readme = "README.md"  -- If you want to include your README as the front page
