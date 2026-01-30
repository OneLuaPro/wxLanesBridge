/*
MIT License

Copyright (c) 2026 Kritzel Kratzel for OneLuaPro

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in 
the Software without restriction, including without limitation the rights to 
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all 
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

/**
 * @module wxLanesBridge
 *
 * Module for bridging wxLua and Lua Lanes.
 *
 * **COMPATIBILITY REQUIREMENT**
 * This module requires a custom-built version of wxLua. You must manually 
 * enable `wxThreadEvent` support in the following binding file:
 * `wxLua/bindings/wxwidgets/wxcore_event.i`
 *
 * Add this code to the file ...
 *
 *    class %delete wxThreadEvent : public wxEvent
 *    {
 *        %wxEventType wxEVT_THREAD
 *        wxThreadEvent(wxEventType eventType = wxEVT_THREAD, int id = wxID_ANY);
 *        long GetExtraLong();
 *        int GetInt();
 *        wxString GetString();
 *        void SetExtraLong(long extraLong);
 *        void SetInt(int intCommand);
 *        void SetString(const wxString &string);
 *    };
 *
 * ... and regenerate the bindings before building wxLua.
 * Without this modification, the GUI thread cannot process bridge events.
 */

#include <lua.hpp>
#include <wx/wx.h>
#define _VERSION "wxLanesBridge 1.0"
wxEventType s_defaultEventID = wxID_ANY;

/**
 * Initializes the wxLanesBridge module by setting the process-wide 
 * `wxThreadEvent` ID. 
 * 
 * This is a necessary requirement when both the wxLua and
 * wxLanesBridge DLLs are linked against a static version of the wxWidgets library.
 * In this configuration, each DLL maintains its own static wxWidgets instance.
 * 
 * Since wxWidgets' internal event IDs (declared via `wxDECLARE_EVENT()` and 
 * `wxDEFINE_EVENT()`) are generated dynamically at runtime using `wxNewEventType()`,
 * the integer values for constants like `wxEVT_THREAD` may differ between the two 
 * DLLs depending on the order of initialization.
 *
 * This function resolves the 
 * discrepancy by injecting the correct ID from the main GUI thread into the 
 * bridge's shared memory. It must be called once before any lane attempts 
 * to use the bridge, ideally immediately after the initial `require` statement, 
 * as shown in the usage example.
 * @function bridge.init
 * @tparam integer id The runtime-defined `wxEVT_THREAD` ID from wxLua.
 * @treturn table self The bridge module table to allow for method chaining.
 * @usage
 * -- In main GUI thread
 * local wx = require("wx")
 * local bridge = require("wxLanesBridge").init(wx.wxEVT_THREAD)
 *
 * -- In worker thread (static ID is already shared in process memory)
 * local bridge = require("wxLanesBridge")
 */
static int init(lua_State* L) {
  // 1. Extract the Event ID from the first argument and store it.
  //    This static value is shared across the entire process (and all lanes)
  //    because that's how static variables in a DLL behave.
  s_defaultEventID = (wxEventType)luaL_checkinteger(L, 1);

  // 2. Return the module table to Lua to allow for chaining.
  //    First, try to find the module in the global table.
  lua_getglobal(L, "wxLanesBridge"); 

  // 3. If it's not global (common in Lua 5.2+), fetch it from the package.loaded table.
  if (lua_isnil(L, -1)) {
    lua_pop(L, 1); // Remove the nil
    lua_getfield(L, LUA_REGISTRYINDEX, "_LOADED");
    lua_getfield(L, -1, "wxLanesBridge");
    lua_remove(L, -2); // Remove the _LOADED table, leaving the module on top
  }

  // Return 1 value (the module table)
  return 1;   
}

/**
 * Extracts the raw C++ memory address from a wxLua userdata object.
 *
 * This function is essential for cross-thread communication with Lua Lanes. 
 * Since complex wxLua userdata objects cannot be safely shared between threads 
 * (Lanes), this function converts them into 'lightuserdata' (a raw C pointer).
 *
 * @function bridge.getPointer
 * @tparam userdata obj The wxLua object (e.g., a wxWindow, wxFrame, or wxButton).
 * @treturn lightuserdata|nil The raw C++ pointer address, or nil if the extraction fails.
 * @raise Throws an error if the bridge has not been initialized via @{bridge.init}.
 * @usage
 * -- Get a thread-safe pointer to pass into a Lane
 * local frame = wx.wxFrame(wx.NULL, wx.wxID_ANY, "My Frame")
 * local ptr = bridge.getPointer(frame)
 * -- The 'ptr' can now be safely passed as an argument to a lane function.
 */
static int getPointer(lua_State* L) {
  if (s_defaultEventID == wxID_ANY) {
    // throw an error as early as possible if wxLanesBridge is not initalized
    return luaL_error(L, "wxLanesBridge must be initialized before use.");
  }
  if (lua_isuserdata(L, 1)) {
    // wxLua userdata blocks contain the pointer to the C++ object at the very beginning.
    // ud is the address of the pointer
    void* ud = lua_touserdata(L, 1);
    if (ud) {
      // Dereference the userdata block to get the actual C++ object address.
      // Push it as lightuserdata, which is a plain pointer value and Lane-safe.
      lua_pushlightuserdata(L, *(void**)ud);
      return 1;
    }
  }
  return 0; // Return nil if extraction fails
}

/**
 * Posts a wxThreadEvent to the main GUI thread.
 * 
 * This function sends an event to the specified wxWidgets object pointer. It requires 
 * the bridge to be initialized via @{bridge.init}. The optional data table allows passing 
 * additional information to the event handler in the GUI thread.
 *
 * @function bridge.postEvent
 * @tparam lightuserdata objPtr The pointer to the target wxLua object (received from the GUI thread).
 * @tparam[opt] table data Optional data table to populate the event fields:
 * @tparam[opt] string data.s Maps to `event:SetString()` (UTF-8 supported).
 * @tparam[opt] integer data.i Maps to `event:SetInt()` (standard command integer).
 * @tparam[opt] integer data.l Maps to `event:SetExtraLong()` (useful for timestamps or 32-bit IDs).
 * @treturn nil
 * @raise Throws an error if Argument 1 is not lightuserdata, Argument 2 is not a table, or if the bridge has not been initialized.
 * @usage
 * -- Example: Sending a complex update from a worker lane
 * bridge.postEvent(objPtr, { 
 *     s = "Calculation finished", 
 *     i = 100, 
 *     l = os.time() 
 * })
 *
 * -- Example: Just triggering the event ("ringing the doorbell")
 * bridge.postEvent(objPtr)
 */
static int postEvent(lua_State* L) {
  // Checck number of arguments (may be 1 or 2)
  int numArgs = lua_gettop(L);
  if (numArgs < 1 || numArgs > 2) {
    return luaL_error(L, "wxLanesBridge: Wrong argument count.");
  }

  // Ensure the bridge was initialized
  if (s_defaultEventID == wxID_ANY) {
    return luaL_error(L, "wxLanesBridge: Error - Call init() before postEvent().");
  }
  
  // First (mandatory) argument must be lightuserdata
  if (!lua_islightuserdata(L, 1)) {
    return luaL_error(L, "wxLanesBridge: Argument 1 must be lightuserdata (e.g. a wxWindow pointer)");
  }

  // Second (optional) argument must be a tabel
  if (numArgs == 2 && !lua_istable(L, 2)) {
    return luaL_error(L, "wxLanesBridge: Optional argument 2 must be a table.");
  }
  
  // Get address of target widget to post the event to
  wxWindow* win = (wxWindow*)lua_touserdata(L, 1);
  if (!win) return 0; // Safety check
  
  // Create new event on C++ stack
  wxThreadEvent event(s_defaultEventID, wxID_ANY);

  // Optional argument 2: The data table { s="...", i=..., l=... }
  // Table indices s, i, and l are aligned to the available wxWidgets
  // member functions for an wxThreadEvent:
  // void SetExtraLong(long extraLong);
  // void SetInt(int intCommand);
  // void SetString(const wxString &string);
  if (lua_istable(L, 2)) {
    // [s]tring
    lua_getfield(L, 2, "s");
    if (lua_isstring(L, -1)) {
      event.SetString(wxString::FromUTF8(lua_tostring(L, -1)));
    }
    lua_pop(L, 1);	// pops the string or nil

    // [i]nteger (intCommand)
    lua_getfield(L, 2, "i");
    if (lua_isnumber(L, -1)) {
      event.SetInt((int)lua_tointeger(L, -1));
    }
    lua_pop(L, 1);	// pops the integer or nil

    // [l]ong (extraLong)
    lua_getfield(L, 2, "l");
    if (lua_isnumber(L, -1)) {
      event.SetExtraLong((long)lua_tointeger(L, -1));
    }
    lua_pop(L, 1);	// pops the extraLong or nil
  }
  // Post event (wxWidgets will create an internal copy for the queue)
  wxPostEvent(win, event);
  
  return 0;
}

static const luaL_Reg bridge_funcs[] = {
  {"init", init},
  {"getPointer", getPointer},
  {"postEvent", postEvent},
  {NULL, NULL}
};

extern "C" LUA_API int luaopen_wxLanesBridge(lua_State* L) {
  luaL_newlib(L, bridge_funcs);
  lua_pushliteral(L,_VERSION);
  lua_setfield(L,-2,"_VERSION");
  return 1;
}
