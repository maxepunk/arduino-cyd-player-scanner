#pragma once

/**
 * @file Screen.h
 * @brief Base class for all UI screens in ALNScanner v5.0
 *
 * Implements the Template Method design pattern for screen rendering lifecycle.
 * All concrete screen implementations inherit from this base class.
 *
 * Key Features:
 * - Polymorphic screen behavior via virtual functions
 * - Consistent render lifecycle (pre, render, post)
 * - Pure virtual interface enforces implementation
 * - Header-only for zero runtime overhead
 *
 * Design Pattern: Template Method
 * - render() is the template method (non-virtual, final)
 * - onPreRender(), onRender(), onPostRender() are hook methods (virtual)
 * - Subclasses override hook methods to customize behavior
 *
 * Usage Example:
 * @code
 * class MyScreen : public Screen {
 * protected:
 *     void onRender(DisplayDriver& display) override {
 *         auto& tft = display.getTFT();
 *         tft.fillScreen(TFT_BLACK);
 *         tft.println("Hello World");
 *     }
 * };
 *
 * MyScreen screen;
 * screen.render(display);  // Calls: onPreRender ’ onRender ’ onPostRender
 * @endcode
 */

#include "../hal/DisplayDriver.h"

namespace ui {

/**
 * @class Screen
 * @brief Abstract base class for all UI screens
 *
 * Provides consistent rendering lifecycle for all screens:
 * 1. onPreRender() - Setup (optional)
 * 2. onRender() - Main rendering (required)
 * 3. onPostRender() - Cleanup (optional)
 *
 * Subclasses MUST implement:
 * - onRender(DisplayDriver&) - Pure virtual function
 *
 * Subclasses MAY override:
 * - onPreRender(DisplayDriver&) - Default: no-op
 * - onPostRender(DisplayDriver&) - Default: no-op
 *
 * Thread Safety:
 * - All render methods must be called from main loop (Core 1)
 * - DisplayDriver handles internal TFT locking
 * - No shared state between screen instances
 */
class Screen {
public:
    /**
     * @brief Virtual destructor for proper cleanup
     *
     * Ensures derived class destructors are called correctly
     * when deleting through base class pointer.
     */
    virtual ~Screen() = default;

    /**
     * @brief Render the screen using template method pattern
     * @param display Reference to DisplayDriver singleton
     *
     * This is the template method - it defines the rendering algorithm:
     * 1. Call onPreRender() for setup
     * 2. Call onRender() for main content
     * 3. Call onPostRender() for cleanup
     *
     * This method is NOT virtual - subclasses cannot override it.
     * Instead, subclasses override the hook methods (onPreRender, onRender, onPostRender).
     *
     * Usage:
     * @code
     * ReadyScreen readyScreen(true, false);
     * readyScreen.render(display);  // Calls the template method
     * @endcode
     */
    void render(hal::DisplayDriver& display) {
        onPreRender(display);
        onRender(display);
        onPostRender(display);
    }

protected:
    /**
     * @brief Hook method: Pre-render setup (optional)
     * @param display Reference to DisplayDriver
     *
     * Called before onRender(). Override to:
     * - Clear screen
     * - Set default colors/fonts
     * - Acquire resources
     *
     * Default implementation: No-op
     */
    virtual void onPreRender(hal::DisplayDriver& display) {
        // Default: Do nothing
        // Subclasses can override if needed
    }

    /**
     * @brief Hook method: Main render implementation (REQUIRED)
     * @param display Reference to DisplayDriver
     *
     * Pure virtual function - subclasses MUST implement.
     *
     * This is where the actual screen content is drawn:
     * - Text rendering
     * - Image display
     * - Shape drawing
     * - Color changes
     *
     * Example:
     * @code
     * void onRender(DisplayDriver& display) override {
     *     auto& tft = display.getTFT();
     *     tft.fillScreen(TFT_BLACK);
     *     tft.setCursor(0, 0);
     *     tft.setTextColor(TFT_WHITE);
     *     tft.println("My Screen Content");
     * }
     * @endcode
     */
    virtual void onRender(hal::DisplayDriver& display) = 0;

    /**
     * @brief Hook method: Post-render cleanup (optional)
     * @param display Reference to DisplayDriver
     *
     * Called after onRender(). Override to:
     * - Release resources
     * - Log rendering time
     * - Trigger animations
     *
     * Default implementation: No-op
     */
    virtual void onPostRender(hal::DisplayDriver& display) {
        // Default: Do nothing
        // Subclasses can override if needed
    }
};

} // namespace ui

/**
 * DESIGN NOTES
 *
 * 1. TEMPLATE METHOD PATTERN
 *    - render() is the template method (defines algorithm structure)
 *    - onPreRender(), onRender(), onPostRender() are hook methods
 *    - Subclasses customize behavior by overriding hook methods
 *    - Ensures consistent rendering lifecycle across all screens
 *
 * 2. PURE VIRTUAL INTERFACE
 *    - onRender() is pure virtual (= 0)
 *    - Screen class cannot be instantiated directly
 *    - Enforces implementation in all concrete screen classes
 *    - Compile-time error if subclass forgets to implement
 *
 * 3. OPTIONAL HOOKS
 *    - onPreRender() and onPostRender() have default no-op implementations
 *    - Subclasses only override if needed
 *    - Reduces boilerplate in simple screens
 *
 * 4. HEADER-ONLY IMPLEMENTATION
 *    - All methods inline (zero runtime overhead)
 *    - No .cpp file needed (simplifies Arduino CLI compilation)
 *    - Virtual functions have minimal overhead (vtable lookup)
 *
 * 5. NAMESPACE ORGANIZATION
 *    - All UI classes in ui:: namespace
 *    - Prevents naming conflicts with HAL and service layers
 *    - Clear separation of concerns
 *
 * 6. DEPENDENCY INJECTION
 *    - DisplayDriver passed as parameter (not global variable)
 *    - Enables unit testing with mock display
 *    - Follows SOLID principles (Dependency Inversion)
 *
 * 7. SCREEN IMPLEMENTATIONS
 *    Concrete screens that inherit from this base class:
 *    - ReadyScreen - Main idle screen
 *    - StatusScreen - Connection/queue status
 *    - TokenDisplayScreen - Token image + audio
 *    - ProcessingScreen - "Sending..." modal for video tokens
 *
 * 8. MEMORY FOOTPRINT
 *    - Base class: 4 bytes (vtable pointer only)
 *    - Derived classes: 4 bytes (vtable) + member variables
 *    - No heap allocations
 *    - Screens typically created on stack
 *
 * 9. THREAD SAFETY
 *    - All screens must be rendered from main loop (Core 1)
 *    - DisplayDriver handles TFT SPI bus locking
 *    - No shared mutable state between screens
 *    - Each render() call is atomic (start to finish)
 *
 * 10. FUTURE ENHANCEMENTS (NOT IMPLEMENTED)
 *     - Animation support (update() method for frame-by-frame)
 *     - Touch event handling (onTouch() hook)
 *     - Screen transitions (fade in/out)
 *     - Dirty region tracking (partial redraws)
 *     - Screen caching (render to buffer, then blit)
 */
