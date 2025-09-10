import turtle
import math
import time

rotateSpeed = 0.01

class Point4D:
    def __init__(self, x, y, z, w):
        self.x = x
        self.y = y
        self.z = z
        self.w = w

def rotate4D(p, angleXW, angleYW, angleZW):
    sinXW, cosXW = math.sin(angleXW), math.cos(angleXW)
    sinYW, cosYW = math.sin(angleYW), math.cos(angleYW)
    sinZW, cosZW = math.sin(angleZW), math.cos(angleZW)

    newX = p.x * cosXW - p.w * sinXW
    newW = p.x * sinXW + p.w * cosXW
    p.x, p.w = newX, newW

    newY = p.y * cosYW - p.w * sinYW
    newW = p.y * sinYW + p.w * cosYW
    p.y, p.w = newY, newW

    newZ = p.z * cosZW - p.w * sinZW
    newW = p.z * sinZW + p.w * cosZW
    p.z, p.w = newZ, newW

def project_and_draw(t, points, edges):
    projected_points = []
    for p in points:
        distance = 3
        px = p.x / (distance - p.w)
        py = p.y / (distance - p.w)
        screen_x = int(px * 200)
        screen_y = int(py * 200)
        projected_points.append((screen_x, screen_y))
    
    t.clear()
    t.penup()
    t.goto(projected_points[0])
    t.pendown()
    for edge in edges:
        p1, p2 = projected_points[edge[0]], projected_points[edge[1]]
        t.penup()
        t.goto(p1[0], -p1[1])
        t.pendown()
        t.goto(p2[0], -p2[1])
    t.getscreen().update()

def main():
    screen = turtle.Screen()
    screen.bgcolor("black")
    screen.tracer(0, 0)

    t = turtle.Turtle()
    t.hideturtle()
    t.color("white")
    t.speed(0)
    t.penup()
    t.goto(0, 0)
    t.pendown()

    points = [
        Point4D(-1, -1, -1, -1), Point4D(-1, -1, -1, 1), Point4D(-1, -1, 1, -1), Point4D(-1, -1, 1, 1),
        Point4D(-1, 1, -1, -1), Point4D(-1, 1, -1, 1), Point4D(-1, 1, 1, -1), Point4D(-1, 1, 1, 1),
        Point4D(1, -1, -1, -1), Point4D(1, -1, -1, 1), Point4D(1, -1, 1, -1), Point4D(1, -1, 1, 1),
        Point4D(1, 1, -1, -1), Point4D(1, 1, -1, 1), Point4D(1, 1, 1, -1), Point4D(1, 1, 1, 1)
    ]

    edges = [
        (0, 1), (0, 2), (0, 4), (1, 3), (1, 5), (2, 3), (2, 6), (3, 7),
        (4, 5), (4, 6), (5, 7), (6, 7), (8, 9), (8, 10), (8, 12), (9, 11),
        (9, 13), (10, 11), (10, 14), (11, 15), (12, 13), (12, 14), (13, 15), (14, 15),
        (0, 8), (1, 9), (2, 10), (3, 11), (4, 12), (5, 13), (6, 14), (7, 15)
    ]

    angleXW = angleYW = angleZW = 0

    while True:
        rotated_points = [Point4D(p.x, p.y, p.z, p.w) for p in points]
        for p in rotated_points:
            rotate4D(p, angleXW, angleYW, angleZW)
        
        project_and_draw(t, rotated_points, edges)
        
        angleXW += rotateSpeed
        angleYW += rotateSpeed
        angleZW += rotateSpeed
        
        time.sleep(0.016)

main()
