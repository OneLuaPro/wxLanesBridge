# wxLanesBridge
Module for bridging wxLua and Lua Lanes.

**Note:** This module requires a custom-built version of [wxLua](https://github.com/pkulchenko/wxlua). You must manually enable `wxThreadEvent` support in the following binding file `wxLua/bindings/wxwidgets/wxcore_event.i`. Add this code to the file ...

```c++
class %delete wxThreadEvent : public wxEvent
 {
     %wxEventType wxEVT_THREAD
     wxThreadEvent(wxEventType eventType = wxEVT_THREAD, int id = wxID_ANY);
     long GetExtraLong();
     int GetInt();
     wxString GetString();
     void SetExtraLong(long extraLong);
     void SetInt(int intCommand);
     void SetString(const wxString &string);
 };
```

... and regenerate the bindings before building [wxLua](https://github.com/pkulchenko/wxlua). Without this modification, the GUI thread cannot process bridge events.

## Application Programming Interface

The module comprises three functions callable from Lua:

- `wxLanesBridge.init()`
- `wxLanesBridge.getPointer()`
- `wxLanesBridge.postEvent()`

### Function `wxLanesBridge.init()`

This function initializes the `wxLanesBridge` module by setting the process-wide `wxThreadEvent` ID. This is a necessary requirement when both the `wxLua` and `wxLanesBridge` DLLs are linked against a static version of the `wxWidgets` library, as it is the case in the [OneLuaPro](https://github.com/OneLuaPro) software distribution. In this configuration, each DLL maintains its own static `wxWidgets` instance.

Since wxWidgets' internal event IDs (declared via `wxDECLARE_EVENT()` and `wxDEFINE_EVENT()`) are generated dynamically at runtime using `wxNewEventType()`, the integer values for constants like `wxEVT_THREAD` may differ between the two DLLs depending on the order of initialization.

This function resolves the discrepancy by injecting the correct ID from the main GUI thread into the bridge's shared memory. It must be called once before any lane attempts to use the bridge, ideally immediately after the initial `require` statement, as shown in the following example.

```lua
-- In main GUI thread
local wx = require("wx")
local bridge = require("wxLanesBridge").init(wx.wxEVT_THREAD)

-- In worker thread(s) (static ID is already shared in process memory)
local bridge = require("wxLanesBridge")
```

### Function `wxLanesBridge.getPointer()`

This function extracts the raw C++ memory address from a `wxLua` userdata object and is essential for cross-thread communication with Lua Lanes. Since complex wxLua userdata objects cannot be safely shared between threads (Lanes), this function converts them into `lightuserdata` (a raw C pointer).

```lua
-- Get a thread-safe pointer to pass into a Lane
local frame = wx.wxFrame(wx.NULL, wx.wxID_ANY, "My Frame")
local ptr = bridge.getPointer(frame)
```

### Function `wxLanesBridge.postEvent()`

This function sends an event to the specified wxWidgets object pointer. It requires the bridge to be initialized via `.init()` The optional data table allows passing additional information to the event handler in the GUI thread.

```lua
-- Example: Just triggering the event ("ringing the doorbell")
bridge.postEvent(objPtr)

-- Example: Sending a complex update from a worker lane
bridge.postEvent(objPtr, { 
    s = "Calculation finished", 
    i = 100, 
    l = os.time() 
})
```

The indices of the optional data table map to the corresponding event handler methods in the GUI thread:

- string `data.s` maps to `event:GetString()` (UTF-8 supported)
- integer `data.i` maps to `event:GetInt()` (standard command integer)
- integer `data.l` maps to `event:GetExtraLong()` (useful for timestamps or 32-bit IDs).

## Example

Check out https://github.com/OneLuaPro/distro/blob/master/DistroCheck.lua and the files in https://github.com/OneLuaPro/distro/tree/master/DistroCheck (in particular: https://github.com/OneLuaPro/distro/blob/master/DistroCheck/appWorkerThread.lua) for a fully-featured example of a multi-threaded wxLua application.

## License

See `https://github.com/OneLuaPro/wxLanesBridge/blob/master/LICENSE`.

