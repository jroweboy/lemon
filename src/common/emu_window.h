// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <tuple>
#include <utility>

#include "common/common_types.h"
#include "common/math_util.h"

#include "core/hle/service/hid/hid.h"

/**
 * Abstraction class used to provide an interface between emulation code and the frontend
 * (e.g. SDL, QGLWidget, GLFW, etc...).
 *
 * Design notes on the interaction between EmuWindow and the emulation core:
 * - Generally, decisions on anything visible to the user should be left up to the GUI.
 *   For example, the emulation core should not try to dictate some window title or size.
 *   This stuff is not the core's business and only causes problems with regards to thread-safety
 *   anyway.
 * - Under certain circumstances, it may be desirable for the core to politely request the GUI
 *   to set e.g. a minimum window size. However, the GUI should always be free to ignore any
 *   such hints.
 * - EmuWindow may expose some of its state as read-only to the emulation core, however care
 *   should be taken to make sure the provided information is self-consistent. This requires
 *   some sort of synchronization (most of this is still a TODO).
 * - DO NOT TREAT THIS CLASS AS A GUI TOOLKIT ABSTRACTION LAYER. That's not what it is. Please
 *   re-read the upper points again and think about it if you don't see this.
 */
class EmuWindow
{
public:
    /// Data structure to store emuwindow configuration
    struct WindowConfig {
        bool    fullscreen;
        int     res_width;
        int     res_height;
        std::pair<unsigned,unsigned> min_client_area_size;
    };

    /// Describes the layout of the window framebuffer (size and top/bottom screen positions)
    struct FramebufferLayout {

        /**
         * Factory method for constructing a default FramebufferLayout
         * @param width Window framebuffer width in pixels
         * @param height Window framebuffer height in pixels
         * @return Newly created FramebufferLayout object with default screen regions initialized
         */
        static FramebufferLayout DefaultScreenLayout(unsigned width, unsigned height);

        unsigned width;
        unsigned height;
        MathUtil::Rectangle<unsigned> top_screen;
        MathUtil::Rectangle<unsigned> bottom_screen;
    };

    /// Swap buffers to display the next frame
    virtual void SwapBuffers() = 0;

    /// Polls window events
    virtual void PollEvents() = 0;

    /// Makes the graphics context current for the caller thread
    virtual void MakeCurrent() = 0;

    /// Releases (dunno if this is the "right" word) the GLFW context from the caller thread
    virtual void DoneCurrent() = 0;

    /**
     * Signal that a touch pressed event has occurred (e.g. mouse click pressed)
     * @param framebuffer_x Framebuffer x-coordinate that was pressed
     * @param framebuffer_y Framebuffer y-coordinate that was pressed
     */
    void TouchPressed(unsigned framebuffer_x, unsigned framebuffer_y);

    /// Signal that a touch released event has occurred (e.g. mouse click released)
    void TouchReleased();

    /**
     * Signal that a touch movement event has occurred (e.g. mouse was moved over the emu window)
     * @param framebuffer_x Framebuffer x-coordinate
     * @param framebuffer_y Framebuffer y-coordinate
     */
    void TouchMoved(unsigned framebuffer_x, unsigned framebuffer_y);

    /**
     * Gets the current accelerometer state (acceleration along each three axis).
     * Axis explained:
     *   +x is the same direction as LEFT on D-pad.
     *   +y is normal to the touch screen, pointing outward.
     *   +z is the same direction as UP on D-pad.
     * Units:
     *   1 unit of return value = 1/512 g (measured by hw test),
     *   where g is the gravitational acceleration (9.8 m/sec2).
     * @note This should be called by the core emu thread to get a state set by the window thread.
     * @todo Implement accelerometer input in front-end.
     * @return std::tuple of (x, y, z)
     */
    std::tuple<s16, s16, s16> GetAccelerometerState() const {
        // stubbed
        return std::make_tuple(0, -512, 0);
    }

    /**
     * Gets the current gyroscope state (angular rates about each three axis).
     * Axis explained:
     *   +x is the same direction as LEFT on D-pad.
     *   +y is normal to the touch screen, pointing outward.
     *   +z is the same direction as UP on D-pad.
     * Orientation is determined by right-hand rule.
     * Units:
     *   1 unit of return value = (1/coef) deg/sec,
     *   where coef is the return value of GetGyroscopeRawToDpsCoefficient().
     * @note This should be called by the core emu thread to get a state set by the window thread.
     * @todo Implement gyroscope input in front-end.
     * @return std::tuple of (x, y, z)
     */
    std::tuple<s16, s16, s16> GetGyroscopeState() const {
        // stubbed
        return std::make_tuple(0, 0, 0);
    }

    /**
     * Gets the coefficient for units conversion of gyroscope state.
     * The conversion formula is r = coefficient * v,
     * where v is angular rate in deg/sec,
     * and r is the gyroscope state.
     * @return float-type coefficient
     */
    f32 GetGyroscopeRawToDpsCoefficient() const {
        return 14.375f; // taken from hw test, and gyroscope's document
    }

    /**
     * Returns currently active configuration.
     * @note Accesses to the returned object need not be consistent because it may be modified in another thread
     */
    const WindowConfig& GetActiveConfig() const {
        return active_config;
    }

    /**
     * Requests the internal configuration to be replaced by the specified argument at some point in the future.
     * @note This method is thread-safe, because it delays configuration changes to the GUI event loop. Hence there is no guarantee on when the requested configuration will be active.
     */
    void SetConfig(const WindowConfig& val) {
        config = val;
    }

    /**
      * Gets the framebuffer layout (width, height, and screen regions)
      * @note This method is thread-safe
      */
    const FramebufferLayout& GetFramebufferLayout() const {
        return framebuffer_layout;
    }

protected:
    EmuWindow() {
        // TODO: Find a better place to set this.
        config.min_client_area_size = std::make_pair(400u, 480u);
        active_config = config;
    }
    virtual ~EmuWindow() {}

    /**
     * Processes any pending configuration changes from the last SetConfig call.
     * This method invokes OnMinimalClientAreaChangeRequest if the corresponding configuration
     * field changed.
     * @note Implementations will usually want to call this from the GUI thread.
     * @todo Actually call this in existing implementations.
     */
    void ProcessConfigurationChanges() {
        // TODO: For proper thread safety, we should eventually implement a proper
        // multiple-writer/single-reader queue...

        if (config.min_client_area_size != active_config.min_client_area_size) {
            OnMinimalClientAreaChangeRequest(config.min_client_area_size);
            config.min_client_area_size = active_config.min_client_area_size;
        }
    }

    /**
     * Update framebuffer layout with the given parameter.
     * @note EmuWindow implementations will usually use this in window resize event handlers.
     */
    void NotifyFramebufferLayoutChanged(const FramebufferLayout& layout) {
        framebuffer_layout = layout;
    }

    /**
     * Update internal client area size with the given parameter.
     * @note EmuWindow implementations will usually use this in window resize event handlers.
     */
    void NotifyClientAreaSizeChanged(const std::pair<unsigned,unsigned>& size) {
        client_area_width = size.first;
        client_area_height = size.second;
    }

private:
    /**
     * Handler called when the minimal client area was requested to be changed via SetConfig.
     * For the request to be honored, EmuWindow implementations will usually reimplement this function.
     */
    virtual void OnMinimalClientAreaChangeRequest(const std::pair<unsigned,unsigned>& minimal_size) {
        // By default, ignore this request and do nothing.
    }

    FramebufferLayout framebuffer_layout; ///< Current framebuffer layout

    unsigned client_area_width;    ///< Current client width, should be set by window impl.
    unsigned client_area_height;   ///< Current client height, should be set by window impl.

    WindowConfig config;         ///< Internal configuration (changes pending for being applied in ProcessConfigurationChanges)
    WindowConfig active_config;  ///< Internal active configuration

    bool touch_pressed;          ///< True if touchpad area is currently pressed, otherwise false

   /**
    * Clip the provided coordinates to be inside the touchscreen area.
    */
    std::tuple<unsigned,unsigned> ClipToTouchScreen(unsigned new_x, unsigned new_y);
};
