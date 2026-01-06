import pygame
import json
import time
import sys

WIDTH, HEIGHT = 1000, 800
BG_COLOR = (20, 20, 30)       
STREET_COLOR = (60, 60, 80)   
STREET_BORDER = (100, 100, 150)
CAR_COLOR = (255, 50, 50)     
TEXT_COLOR = (255, 255, 255)
PADDING = 50                  

MIN_LAT, MAX_LAT = 0, 0
MIN_LON, MAX_LON = 0, 0

def load_map_bounds(streets):

    global MIN_LAT, MAX_LAT, MIN_LON, MAX_LON
    
    if not streets:
        return

    all_lats = []
    all_lons = []
    
    for s in streets:
        all_lats.append(s['lat_min'])
        all_lats.append(s['lat_max'])
        all_lons.append(s['lon_min'])
        all_lons.append(s['lon_max'])
    
    MIN_LAT = min(all_lats)
    MAX_LAT = max(all_lats)
    MIN_LON = min(all_lons)
    MAX_LON = max(all_lons)
    
    print(f"[Harta] Limite detectate: Lat({MIN_LAT}-{MAX_LAT}), Lon({MIN_LON}-{MAX_LON})")

def to_screen(lat, lon):
    if MAX_LON == MIN_LON or MAX_LAT == MIN_LAT:
        return WIDTH // 2, HEIGHT // 2

    draw_width = WIDTH - (2 * PADDING)
    draw_height = HEIGHT - (2 * PADDING)

    norm_x = (lon - MIN_LON) / (MAX_LON - MIN_LON)
    norm_y = (lat - MIN_LAT) / (MAX_LAT - MIN_LAT)
    
    screen_x = PADDING + int(norm_x * draw_width)
    screen_y = HEIGHT - PADDING - int(norm_y * draw_height)
    
    return screen_x, screen_y

def get_data_from_json(filename):
    try:
        with open(filename, 'r') as f:
            data = json.load(f)
            return data
    except json.JSONDecodeError:
        return [] 
    except FileNotFoundError:
        return [] 
    except Exception as e:
        return []

pygame.init()
screen = pygame.display.set_mode((WIDTH, HEIGHT))
pygame.display.set_caption("Traffic Monitor - Live Map")
font = pygame.font.SysFont("Verdana", 12, bold=True)
title_font = pygame.font.SysFont("Verdana", 24, bold=True)

streets = get_data_from_json('streets.json')
if not streets:
    print("EROARE: Nu am gasit streets.json! Asigura-te ca serverul a pornit.")
    streets = [] 

load_map_bounds(streets)

running = True
clock = pygame.time.Clock()

while running:
    for event in pygame.event.get():
        if event.type == pygame.QUIT:
            running = False

    screen.fill(BG_COLOR)

    title = title_font.render("Sistem Monitorizare Trafic", True, (150, 150, 170))
    screen.blit(title, (20, 20))

    for s in streets:
        x1, y1 = to_screen(s['lat_max'], s['lon_min'])
        x2, y2 = to_screen(s['lat_min'], s['lon_max'])
        
        rect_w = abs(x2 - x1)
        rect_h = abs(y2 - y1)
        
        rect_w = max(rect_w, 10) 
        rect_h = max(rect_h, 10)
        
        top_left_x = min(x1, x2)
        top_left_y = min(y1, y2)
        
        pygame.draw.rect(screen, STREET_COLOR, (top_left_x, top_left_y, rect_w, rect_h))
        pygame.draw.rect(screen, STREET_BORDER, (top_left_x, top_left_y, rect_w, rect_h), 1) # Contur
        
        text = font.render(s['name'], True, (200, 200, 200))
        text_rect = text.get_rect(center=(top_left_x + rect_w/2, top_left_y + rect_h/2))
        screen.blit(text, text_rect)

    cars = get_data_from_json('cars.json')
    
    for car in cars:
        cx, cy = to_screen(car['lat'], car['lng'])
        
        pygame.draw.circle(screen, CAR_COLOR, (cx, cy), 10)
        pygame.draw.circle(screen, (255, 255, 255), (cx, cy), 12, 2)
        
        info_text = f"{car['id']} | {car['speed']}km/h"
        text_surf = font.render(info_text, True, TEXT_COLOR)
        
        screen.blit(text_surf, (cx - 20, cy - 25))

    pygame.display.flip()
    
    clock.tick(30) 

pygame.quit()
sys.exit()