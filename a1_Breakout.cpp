/*
 * Author: Huiting Luo
 */
#include <cstdlib>
#include <unistd.h>
#include <iostream>
#include <string>
#include <stdlib.h>
#include <list>
#include <vector>
#include <map>
#include <sstream>
#include <sys/time.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

using namespace std;

unsigned int FPS = 50;
int paddleSpeed = 40;
int ballSpeed = 5;
int ballsLost = 0;
int paddle_w = 100;
int paddle_h = 10;
int paddleX = 0;
int paddleY = 0;
int windowWidth = 790;
int windowHeight =600;
int lastKnownPaddleX = 0;
long int score = 0;
int level = 1;
unsigned long lastRepaint = 0;
bool gameOver = false;
bool gameWon = false;
bool level_changed = false;
bool startAnimation = false;
int destroyedRectangles = 0;

struct XInfo {
    Display  *display;
    Window   window;
    GC     gc[6];
};
struct BrickInfo {
    int xLocation;
    int yLocation;
} brickInfo;

vector<BrickInfo> bricks_location_vector;
map<pair<int,int>, bool> brickDes;

string to_string(double n)
{
    ostringstream stream;
    stream << n ;
    return stream.str();
}
void clearMap() {
    destroyedRectangles = 0;
    unsigned int s =bricks_location_vector.size();
    for (unsigned int i=0; i< s; i++) {
        BrickInfo tmp  = bricks_location_vector[i];
        pair <int, int> key=make_pair( tmp.xLocation, tmp.yLocation );
        brickDes[ key] = false;
    }
}
void reset() {
    score = 0;
    ballsLost = 0;
    level = 1;
    FPS = 50;
    paddle_w= 100;
    paddle_h = 10;
    ballSpeed = 5;
}

/*
 * An abstract class representing displayable things.
 */
class Displayable {
public:
    virtual void paint( XInfo &xinfo ) = 0;
};

class Bricks : public Displayable {
private:
    int x;
    int y;
public:
    Bricks ( unsigned int windowWidth, unsigned int windowHeight ) { }
    void fillBrick(bool destroyed, XInfo &xinfo, int x, int y) {
        if (!destroyed)
            XFillRectangle( xinfo.display, xinfo.window, xinfo.gc[2], x, y, 70, 30);
        brickInfo.xLocation = x;
        brickInfo.yLocation = y;
        bricks_location_vector.push_back( brickInfo );
    }
    virtual void paint( XInfo &xinfo ) {
        y = 0;
        bricks_location_vector.clear();
        for( x = 2; x < windowWidth; x = x+72) {
            pair<int, int> tmp =make_pair( x,y );
            fillBrick(brickDes[tmp], xinfo, x, y);
            for( y= y+32; y < windowHeight *0.25 ; y = y+32) {
                pair<int, int> tmp1=make_pair( x,y );
                fillBrick(brickDes[ tmp1], xinfo, x, y);
            }
            y = 0;
        }
    }
};

class Ball : public Displayable {
private:
    bool ball_touched_paddle;
    bool ball_touched_brick;
    bool ball_touched_wall;
    int xLocation;
    int yLocation;
    int directionX;
    int directionY;
public:
    Ball( int null ) { }
    virtual void paint( XInfo &xinfo ) {
        if ( !startAnimation ) {
            srand((unsigned)time(0));
            int randomNumber = (rand()%(10-1+1))-1; // rand()%(b-a+1)+a; random between [a, b]
            
            if ( randomNumber%2 ) { // odd number
                directionX = ballSpeed;
            } else {
                directionX = 0-ballSpeed;
            }
            directionY = ballSpeed;
            xLocation = (windowWidth*0.5)-10;
            yLocation = (windowHeight/1.2)-20;
        }
        XFillArc( xinfo.display, xinfo.window, xinfo.gc[3], xLocation, yLocation, 20, 20, 0, 360*64 );
    }
    
    void move( XInfo &xinfo ) {
        //  touches  bricks
        if ( !ball_touched_brick ) {
            unsigned int s =bricks_location_vector.size();
            for (  unsigned int i = 0; i<s ; i++) {
                BrickInfo tmp  = bricks_location_vector[i];
                if (( yLocation >= tmp.yLocation && yLocation <= tmp.yLocation+30) &&
                    xLocation >= tmp.xLocation && xLocation+20 <= tmp.xLocation+72) {
                    pair<int, int> key=make_pair( tmp.xLocation, tmp.yLocation );
                    bool notDes =!brickDes[ key ];
                    if ( notDes ) {
                        directionY = 0-directionY;
                        score = score+100;
                        brickDes[ key] = true;
                        destroyedRectangles++;

                    }
                }
            }
            ball_touched_brick = true;
        } else {
            ball_touched_brick = false;
        }
        //ball hits wall
        if ( !ball_touched_wall ) {
            if ( xLocation+20 >= windowWidth ) {
                directionX = 0-directionX;
            } if ( xLocation <= 0 ) {
                directionX = 0-directionX;
                xLocation = 0;
            } if ( yLocation+20 >= windowHeight ) {
                ballsLost ++;
                if ( ballsLost ==3 )
                    gameOver = true;
                startAnimation = false;
            } if ( yLocation <= 0 ) {
                directionY = 0-directionY;
                yLocation = 0;
            }
            ball_touched_wall = true;
        } else {
            ball_touched_wall = false;
        }
        
        if ( (yLocation+20 == paddleY) &&
            (xLocation+20 >= paddleX) && (xLocation <= paddleX+paddle_w) ) {
            if ( !ball_touched_paddle ) {
                ball_touched_paddle = true;
                directionY = 0-directionY;
                // limit for ballspeed
                if ( directionX+(paddleX-lastKnownPaddleX) <= ballSpeed *2&&
                    directionX+(paddleX-lastKnownPaddleX) >= -ballSpeed*2 ) {
                    directionX = directionX+(paddleX-lastKnownPaddleX);
                }else if ( directionX+(paddleX-lastKnownPaddleX) < -ballSpeed*2 ) {
                    directionX = ballSpeed*2;
                }else if ( directionX+(paddleX-lastKnownPaddleX) > ballSpeed*2 ) {
                    directionX = ballSpeed*2;
                }
            }
        } else {
            ball_touched_paddle = false;
        }
        xLocation = xLocation + directionX;
        yLocation = yLocation + directionY;
    }
};

class Paddle : public Displayable {
private:
    int xLocation;
    int yLocation;
    
public:
    virtual void paint( XInfo &xinfo ) {
        if ( !startAnimation ) {
            xLocation = (windowWidth*0.5)-(paddle_w*0.5);
            yLocation = windowHeight/1.2;
            paddleX = xLocation;
            paddleY = yLocation;
        } else {
            yLocation = windowHeight/1.2;
        }
        lastKnownPaddleX = xLocation;
        XFillRectangle( xinfo.display, xinfo.window, xinfo.gc[1], xLocation, yLocation, paddle_w, paddle_h );
    }
    void moveLeft( XInfo &xinfo ) {
        if ( xLocation < paddleSpeed &&  xLocation > 0) {
            xLocation = 0;
            lastKnownPaddleX = paddleX;
            paddleX = xLocation;
            paddleY = yLocation;
        } else if (xLocation > 0) {
            xLocation = xLocation-paddleSpeed;
            lastKnownPaddleX = paddleX;
            paddleX = xLocation;
            paddleY = yLocation;
        }
    }
    void moveRight( XInfo &xinfo ) {
        if ( xLocation+paddle_w < windowWidth ) {
            if ( xLocation+paddle_w+paddleSpeed > windowWidth) {
                xLocation = windowWidth-paddle_w;
            } else {
                xLocation = xLocation+paddleSpeed;
            }
            lastKnownPaddleX = paddleX;
            paddleX = xLocation;
            paddleY = yLocation;
        }
    }
    void goToLocation ( int x ) {
        if ( ((x-(paddle_w*0.5))+paddle_w <= windowWidth) &&
            (x > (paddle_w*0.5)) ) {
            xLocation = x-(paddle_w*0.5);
        } else if ( x < (paddle_w*0.5) ) {
            xLocation = 0;
        } else if ( (x-(paddle_w*0.5))+paddle_w > windowWidth ) {
            xLocation = windowWidth-paddle_w;
        }
        
        lastKnownPaddleX = paddleX;
        paddleX = xLocation;
        paddleY = yLocation;
    }
    
    Paddle ( int null ) { }
};

list<Displayable *> dList;
Bricks bricks( windowWidth, windowHeight );
Paddle paddle(0);
Ball ball(0);

/*
 * Initialize X and create a window
 */
void initX( int argc, char *argv[], XInfo &xInfo )
{
    xInfo.display = XOpenDisplay( "" );
    if ( !xInfo.display )    {
        cerr <<  "Can't open display." << endl;
    }
    
    int screen = DefaultScreen( xInfo.display );
    unsigned long white = WhitePixel( xInfo.display, screen );
    unsigned long black = BlackPixel( xInfo.display, screen );
    
    xInfo.window = XCreateSimpleWindow( xInfo.display,                   // display where window appears
                                       DefaultRootWindow( xInfo.display ), // window's parent in window tree
                                       100, 100,               // upper left corner location
                                       790, 600,    // size of the window
                                       6,                   // width of window's border
                                       black,                           // window border colour
                                       white );                        // window background colour
    
    XColor my_color;
    Colormap screen_colormap = DefaultColormap(xInfo.display, screen);
    
    int i = 0;
    xInfo.gc[i] = XCreateGC( xInfo.display, xInfo.window, 0, 0 );
    XAllocNamedColor(xInfo.display, screen_colormap, "white", &my_color, &my_color);
    XSetForeground( xInfo.display, xInfo.gc[i], my_color.pixel );
    XSetBackground( xInfo.display, xInfo.gc[i], black );
    XSetFillStyle( xInfo.display, xInfo.gc[i], FillSolid );
    XSetLineAttributes( xInfo.display, xInfo.gc[i],
                       1, LineSolid, CapButt, JoinRound );
    
    i = 1;
    xInfo.gc[i] = XCreateGC( xInfo.display, xInfo.window, 0, 0 );
    XSetForeground( xInfo.display, xInfo.gc[i], black );
    XSetBackground( xInfo.display, xInfo.gc[i], white );
    XSetFillStyle( xInfo.display, xInfo.gc[i], FillSolid);
    XSetLineAttributes( xInfo.display, xInfo.gc[i],
                       1, LineOnOffDash, CapRound, JoinRound );
    // draw bricks
    i = 2;
    xInfo.gc[i] = XCreateGC( xInfo.display, xInfo.window, 0, 0 );
    XAllocNamedColor(xInfo.display, screen_colormap, "DarkSalmon", &my_color, &my_color);
    XSetForeground( xInfo.display, xInfo.gc[i], my_color.pixel );
    XSetBackground( xInfo.display, xInfo.gc[i], white );
    XSetFillStyle( xInfo.display, xInfo.gc[i], FillSolid );
    XSetLineAttributes( xInfo.display, xInfo.gc[i],
                       2, LineSolid, CapRound, JoinMiter );
    
    // draw ball
    i = 3;
    xInfo.gc[i] = XCreateGC( xInfo.display, xInfo.window, 0, 0 );
    XAllocNamedColor(xInfo.display, screen_colormap, "grey", &my_color, &my_color);
    XSetForeground( xInfo.display, xInfo.gc[i], my_color.pixel );
    XSetBackground( xInfo.display, xInfo.gc[i], white );
    XSetFillStyle( xInfo.display, xInfo.gc[i], FillSolid );
    XSetLineAttributes( xInfo.display, xInfo.gc[i],
                       1, LineSolid, CapRound, JoinRound );
    i = 4;
    xInfo.gc[i] = XCreateGC( xInfo.display, xInfo.window, 0, 0 );
    XAllocNamedColor(xInfo.display, screen_colormap, "brown", &my_color, &my_color);
    XSetForeground( xInfo.display, xInfo.gc[i], my_color.pixel );
    i = 5;
    xInfo.gc[i] = XCreateGC( xInfo.display, xInfo.window, 0, 0 );
    XAllocNamedColor(xInfo.display, screen_colormap, "red", &my_color, &my_color);
    XSetForeground( xInfo.display, xInfo.gc[i], my_color.pixel );
    
    // Load fonts
    string font_name = "*-helvetica-*--18-*-*";
    XFontStruct* font_info = XLoadQueryFont(xInfo.display, font_name.data());
    if (!font_info) {
        cerr << "failed loading font "  << endl;
    }
    XSetFont(xInfo.display, xInfo.gc[4], font_info->fid);
    XSetFont(xInfo.display, xInfo.gc[5], font_info->fid);
    
    XSelectInput( xInfo.display, xInfo.window, ExposureMask | KeyPressMask | KeyReleaseMask | PointerMotionMask );
    
    XMapRaised( xInfo.display, xInfo.window );
    
    XFlush(xInfo.display);
    sleep(1);
}

void display_text ( XInfo &xinfo ) {
    
    XDrawString( xinfo.display, xinfo.window, xinfo.gc[4], (windowWidth/2)-118, windowHeight-10, "Press 'a'/'d' to move left/right", 32 );
    XDrawString( xinfo.display, xinfo.window, xinfo.gc[4], (windowWidth/2)-78, windowHeight-30,  "Press space to start", 20 );
    
    string score_string = to_string(score);
    XDrawString( xinfo.display, xinfo.window, xinfo.gc[4], windowWidth-185, windowHeight-50, "Total score: ", 13 );
    XDrawString( xinfo.display, xinfo.window, xinfo.gc[4], windowWidth-70, windowHeight-50, score_string.data(), score_string.length() );
    
    int balls_remaining = (3-ballsLost)-1;
    balls_remaining = balls_remaining<0?0:balls_remaining;
    string balls_string=to_string(balls_remaining);
    XDrawString( xinfo.display, xinfo.window, xinfo.gc[4], windowWidth-185, windowHeight-10, "Balls remaining: ", 17 );
    XDrawString( xinfo.display, xinfo.window, xinfo.gc[4], windowWidth-35, windowHeight-10, balls_string.data(), balls_string.length() );
    
    string level_string =to_string(level) ;
    XDrawString( xinfo.display, xinfo.window, xinfo.gc[4], windowWidth-185, windowHeight-30, "Level: ", 7 );
    XDrawString( xinfo.display, xinfo.window, xinfo.gc[4], windowWidth-125, windowHeight-30, level_string.data(), level_string.length() );
    
    if ( gameOver ) {
        startAnimation = false;
        XDrawString( xinfo.display, xinfo.window, xinfo.gc[5], (windowWidth*0.5)-45, windowHeight*0.5, "GAME OVER", 9 );
        XDrawString( xinfo.display, xinfo.window, xinfo.gc[5], (windowWidth*0.5)-90, (windowHeight*0.5)+30, "Try Again Press space", 21 );
        XDrawString( xinfo.display, xinfo.window, xinfo.gc[5], (windowWidth*0.5)-60, (windowHeight*0.5)+60, "Quit Press 'q'", 14 );
        clearMap();
    }
    
    if ( bricks_location_vector.size()== destroyedRectangles  ) {
        if ( level == 3 ) gameWon = true;
        if ( level < 3) level++;
        level_changed = true;
    }
    
    if ( gameWon ) {
        startAnimation = false;
        XDrawString( xinfo.display, xinfo.window, xinfo.gc[5], (windowWidth*0.5)-45, windowHeight*0.5, "You Won the Game!!!", 19 );
        XDrawString( xinfo.display, xinfo.window, xinfo.gc[5], (windowWidth*0.5)-120, (windowHeight*0.5)+30, "Press space to start a new game", 31 );
        clearMap();
    }
    
    if ( level_changed ) {
        if ( level == 2 ) {
            paddle_w = 90;
            paddle_h = 10;
            ballSpeed = 15;
        } else if ( level == 3 ) {
            paddle_w = 50;
            paddle_h = 10;
            ballSpeed = 20;
        }
        clearMap();
        level_changed = false;
        startAnimation = false;
    }
    XDrawString( xinfo.display, xinfo.window, xinfo.gc[4], 10 ,windowHeight-50, "Author: Huiting Luo (h36luo)", 28 );
    string ball_speed_string = to_string(ballSpeed);
    XDrawString( xinfo.display, xinfo.window, xinfo.gc[4], 10, windowHeight-30, "Ball speed: ", 12 );
    XDrawString( xinfo.display, xinfo.window, xinfo.gc[4], 115, windowHeight-30, ball_speed_string.data(), ball_speed_string.length() );
    
    string fps_string = to_string(FPS);
    XDrawString( xinfo.display, xinfo.window, xinfo.gc[4], 10, windowHeight-10, "FPS: ", 5 );
    XDrawString( xinfo.display, xinfo.window, xinfo.gc[4], 50, windowHeight-10, fps_string.data(), fps_string.length() );
    
}

unsigned long now()
{
    timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000000 + tv.tv_usec;
}


void repaint( XInfo &xinfo) {
    unsigned long endRepaint = now();
    if ( endRepaint - lastRepaint > 1000000/FPS )
    {
        list<Displayable *>::const_iterator begin = dList.begin();
        list<Displayable *>::const_iterator end = dList.end();
        
        XFillRectangle(xinfo.display, xinfo.window, xinfo.gc[0], 0, 0, windowWidth, windowHeight);
        while( begin != end ) {
            Displayable *d = *begin;
            d->paint(xinfo);
            begin++;
        }
        
        display_text( xinfo );
        
        XFlush( xinfo.display );
        
        lastRepaint = now();
    }
}

void handle_keyrelease( XEvent &event )
{
    KeySym key;
    char text[10];
    
    int i = XLookupString( (XKeyEvent *)&event,     // the keyboard event
                          text,                     // buffer when text will be written
                          10,             // size of the text buffer
                          &key,                     // workstation-independent key symbol
                          NULL );                // pointer to a composeStatus structure (unused)
    if (  i == 1 && key == XK_space ) {
        startAnimation = true;
        if ( gameWon ) {
            gameWon = false;
            reset();
        }
        if ( gameOver ) {
            gameOver = false;
            reset();
        }
    }
}

void handle_keypress( XEvent &event, XInfo &xinfo )
{
    KeySym key;
    char text[5];
    
    int i = XLookupString( (XKeyEvent *)&event,     // the keyboard event
                          text,                     // buffer when text will be written
                          5,             // size of the text buffer
                          &key,                     // workstation-independent key symbol
                          NULL );                // pointer to a composeStatus structure (unused)
    if ( i == 1) {
        if (startAnimation) {
            if ( text[0] == 'a'  ) {
                paddle.moveLeft( xinfo );
            }
            else if ( text[0] == 'd' ) {
                paddle.moveRight( xinfo );
            }
        }
        if ( text[0] == 'q') {
            cerr << " Game terminated." << endl;
            XCloseDisplay(xinfo.display);
            exit(0);
        }
    }
}

void handle_expose( XInfo &xinfo )
{
    XWindowAttributes windowInfo;
    XGetWindowAttributes(xinfo.display, xinfo.window, &windowInfo);
    windowWidth = windowInfo.width;
    windowHeight = windowInfo.height;
    
    if ( startAnimation ) {
        ballsLost ++;
        if ( ballsLost == 3 ) gameOver = true;
    }
    startAnimation = false;
    paddleSpeed = windowWidth*0.05;
    clearMap();
}

void eventLoop( XInfo &xinfo )
{
    dList.push_front(&bricks);
    dList.push_front(&paddle);
    dList.push_front(&ball);
    
    XEvent event;
    unsigned long last = 0;
    int done = false;
    
    while ( !done )
    {
        if ( XPending( xinfo.display ) > 0 )
        {
            XMoveWindow(xinfo.display, xinfo.window, 300, 100);
            XNextEvent( xinfo.display, &event );
            switch ( event.type )
            {
                case KeyPress:
                    handle_keypress( event, xinfo );
                    break;
                case KeyRelease:
                    handle_keyrelease( event );
                    break;
                case Expose:
                    handle_expose( xinfo );
                    break;
                case MotionNotify:
                    if (startAnimation) paddle.goToLocation( event.xbutton.x );
                    break;
                default:
                    break;
            }
        }
        unsigned long end = now();
        if ( end - last > 1000000/30 )
        {
            if ( startAnimation ) {
                ball.move ( xinfo );
            }
            repaint( xinfo );
            last = now();
        } else if ( XPending(xinfo.display) == 0 ) {
            usleep(1000000/30 - (end - last));
        }
    }
}
int main( int argc, char *argv[] )
{
    XInfo xInfo;
    initX( argc, argv, xInfo );
    if(argc == 3){
        istringstream convert(argv[1]);
        istringstream convert1(argv[2]);
        convert>>FPS;
        convert1>> ballSpeed;
    }
    eventLoop(xInfo);
    XCloseDisplay(xInfo.display);
}
