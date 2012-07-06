/*
	Copyright (C) 2012 Alexander Overvoorde

	Permission is hereby granted, free of charge, to any person obtaining a copy of
	this software and associated documentation files (the "Software"), to deal in
	the Software without restriction, including without limitation the rights to
	use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
	the Software, and to permit persons to whom the Software is furnished to do so,
	subject to the following conditions:

	The above copyright notice and this permission notice shall be included in all
	copies or substantial portions of the Software.

	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
	FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
	COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
	IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
	CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE
*/

// The X11 implementation of the window class is heavily inspired by the implementation in SFML 2.
// A huge thanks goes to Laurent Gomila for developing that code.

// TODO: Handle key events and window styles like fullscreen

#include <GL/Window/Window.hpp>

#ifdef OOGL_PLATFORM_LINUX

namespace GL
{
	Window::Window( uint width, uint height, const std::string& title, uint style )
	{
		display = XOpenDisplay( NULL );
		int screen = DefaultScreen( display );

		XSetWindowAttributes attributes;
		attributes.event_mask = FocusChangeMask | ButtonPressMask | ButtonReleaseMask | ButtonMotionMask | PointerMotionMask | KeyPressMask | KeyReleaseMask | StructureNotifyMask | EnterWindowMask | LeaveWindowMask;
		attributes.override_redirect = 0;

		int x, y;
		x = ( DisplayWidth( display, screen ) - width ) / 2;
		y = ( DisplayHeight( display, screen ) - height ) / 2;

		// Create window on server
		::Window desktop = RootWindow( display, screen );
		int depth = DefaultDepth( display, screen );
		window = XCreateWindow( display, desktop, x, y, width, height, 0, depth, InputOutput, DefaultVisual( display, screen ), CWEventMask | CWOverrideRedirect, &attributes );

		// Title
		XStoreName( display, window, title.c_str() );

		// Window style
		Atom windowHints = XInternAtom( display, "_MOTIF_WM_HINTS", false );

		struct WMHints
		{
			unsigned long Flags;
			unsigned long Functions;
			unsigned long Decorations;
			long InputMode;
			unsigned long State;
		};

		static const unsigned long MWM_HINTS_FUNCTIONS = 1 << 0;
		static const unsigned long MWM_HINTS_DECORATIONS = 1 << 1;
		static const unsigned long MWM_DECOR_BORDER = 1 << 1;
		static const unsigned long MWM_DECOR_TITLE = 1 << 3;
		static const unsigned long MWM_DECOR_MENU = 1 << 4;
		static const unsigned long MWM_DECOR_MINIMIZE = 1 << 5;
		static const unsigned long MWM_FUNC_MOVE = 1 << 2;
		static const unsigned long MWM_FUNC_MINIMIZE = 1 << 3;
		static const unsigned long MWM_FUNC_CLOSE = 1 << 5;

		WMHints hints;
		hints.Flags = MWM_HINTS_FUNCTIONS | MWM_HINTS_DECORATIONS;
		hints.Decorations = 0;
		hints.Functions = 0;

		hints.Decorations |= MWM_DECOR_BORDER | MWM_DECOR_TITLE | MWM_DECOR_MINIMIZE | MWM_DECOR_MENU;
		hints.Functions |= MWM_FUNC_MOVE | MWM_FUNC_MINIMIZE;
		hints.Functions |= MWM_FUNC_CLOSE;

		const unsigned char* ptr = reinterpret_cast<const unsigned char*>( &hints );
		XChangeProperty( display, window, windowHints, windowHints, 32, PropModeReplace, ptr, 5 );

		// Initialize input
		close = XInternAtom( display, "WM_DELETE_WINDOW", false );
		XSetWMProtocols( display, window, &close, 1 );

		// Show window
		XMapWindow( display, window );
		XFlush( display );

		// Initialize window properties
		this->x = x;
		this->y = y;
		this->width = width;
		this->height = height;
		this->open = true;
		this->mousex = 0;
		this->mousey = 0;
		memset( this->mouse, 0, sizeof( this->mouse ) );
		memset( this->keys, 0, sizeof( this->keys ) );
	}

	Window::~Window()
	{
		XDestroyWindow( display, window );
		XFlush( display );

		XCloseDisplay( display );
	}

	void Window::SetPos( int x, int y )
	{
		if ( !open ) return;
		XMoveWindow( display, window, x, y );
    	XFlush( display );
	}

	void Window::SetSize( uint width, uint height )
	{
		if ( !open ) return;
		XResizeWindow( display, window, width, height );
    	XFlush( display );
	}

	void Window::SetTitle( const std::string& title )
	{
		if ( !open ) return;
		XStoreName( display, window, title.c_str() );
	}

	void Window::SetVisible( bool visible )
	{
		if ( !open ) return;
		
		if ( visible )
        	XMapWindow( display, window );
    	else
        	XUnmapWindow( display, window );

    	XFlush( display );
	}

	void Window::Close()
	{
		XDestroyWindow( display, window );
		XFlush( display );
		open = false;
	}

	bool Window::GetEvent( Event& ev )
	{
		// Fetch new events
		XEvent event;
		while ( XCheckIfEvent( display, &event, &CheckEvent, reinterpret_cast<XPointer>( window ) ) )
		{
			WindowEvent( event );
		}

		// Return oldest event - if available
		if ( events.empty() ) return false;
		
		ev = events.front();
		events.pop();

		return true;
	}

	void Window::WindowEvent( const XEvent& event )
	{
		Event ev;
		ev.Type = 0;

		// Translate XEvent to Event
		uint button = 0;
		switch ( event.type )
		{
			case ClientMessage:
				if ( ( event.xclient.format == 32 ) && ( event.xclient.data.l[0] ) == static_cast<long>( close ) )
				{
					open = false;
					ev.Type = Event::Close;
				}
				break;

			case ConfigureNotify:
				if ( (uint)event.xconfigure.width != width || (uint)event.xconfigure.height != height )
				{
					width = event.xconfigure.width;
					height = event.xconfigure.height;

					if ( events.empty() ) {
						ev.Type = Event::Resize;
						ev.Window.Width = width;
						ev.Window.Height = height;
					} else if ( events.back().Type == Event::Resize ) {
						events.back().Window.Width = width;
						events.back().Window.Height = height;
					}
				} else if ( event.xconfigure.x != x || event.xconfigure.y != y ) {
					x = event.xconfigure.x;
					y = event.xconfigure.y;

					if ( events.empty() ) {
						ev.Type = Event::Move;
						ev.Window.X = x;
						ev.Window.Y = y;
					} else if ( events.back().Type == Event::Move ) {
						events.back().Window.X = x;
						events.back().Window.Y = y;
					}
				}
				break;

			case FocusIn:
				ev.Type = Event::Focus;
				focus = true;
				break;

			case FocusOut:
				ev.Type = Event::Blur;
				focus = false;
				break;

			// TODO: KeyDown, KeyUp

			case ButtonPress:
				button = event.xbutton.button;

				if ( button == Button1 || button == Button2 || button == Button3 )
				{
					mousex = event.xbutton.x;
					mousey = event.xbutton.y;

					ev.Type = Event::MouseDown;
					ev.Mouse.X = mousex;
					ev.Mouse.Y = mousey;

					if ( button == Button1 ) ev.Mouse.Button = MouseButton::Left;
					else if ( button == Button2 ) ev.Mouse.Button = MouseButton::Middle;
					else if ( button == Button3 ) ev.Mouse.Button = MouseButton::Right;
				}
				break;

			case ButtonRelease:
				button = event.xbutton.button;

				if ( button == Button1 || button == Button2 || button == Button3 )
				{
					mousex = event.xbutton.x;
					mousey = event.xbutton.y;

					ev.Type = Event::MouseUp;
					ev.Mouse.X = mousex;
					ev.Mouse.Y = mousey;

					if ( button == Button1 ) ev.Mouse.Button = MouseButton::Left;
					else if ( button == Button2 ) ev.Mouse.Button = MouseButton::Middle;
					else if ( button == Button3 ) ev.Mouse.Button = MouseButton::Right;
				} else if ( button == Button4 || button == Button5 ) {
					mousex = event.xbutton.x;
					mousey = event.xbutton.y;

					ev.Type = Event::MouseWheel;
					ev.Mouse.Delta = button == Button4 ? 1 : -1;;
					ev.Mouse.X = mousex;
					ev.Mouse.Y = mousey;
				}
				break;

			case MotionNotify:
				mousex = event.xmotion.x;
				mousey = event.xmotion.y;

				ev.Type = Event::MouseMove;
				ev.Mouse.X = mousex;
				ev.Mouse.Y = mousey;
				break;
		}

		// Add event to internal queue
		if ( ev.Type != 0 )
			events.push( ev );
	}

	Bool Window::CheckEvent( Display*, XEvent* event, XPointer userData )
	{
		return event->xany.window == reinterpret_cast< ::Window >( userData );
	}
}

#endif