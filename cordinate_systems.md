# World space

0,0 is at the center of a world, fixed relitive to the world.

X and Y are horisontal, Z is vertical.

```
Z
|  Y
| / 
|/___X
```

# Camera space

World space, but translated and rotated so that the camera is always at 0,0 and facing +y

X and Y are horisontal, Z is vertical.

```
Z
|  Y
| / 
|/___X
```

# Normalized screen space

2D space, used to abstractly represent the screen 0,0 is the center of the screen.

Cordinates are from -1 to 1

X is horisontal, Y is vertical

```
Y
|
| 
|____Z
```

# Pixel space

Physical screen space, 1 unit = one pixel

0, 0 is top left.


```
 ____ X
|
|
|
Y
```

# Data flow

World space (world) -> Camera space (camera transform) -> Screen space (projection) -> Pixel space (scaling)
