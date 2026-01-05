import pygame
import json
import time
import sys

# --- CONFIGURĂRI GRAFICE ---
WIDTH, HEIGHT = 1000, 800
BG_COLOR = (20, 20, 30)       # Albastru foarte inchis (stil Radar)
STREET_COLOR = (60, 60, 80)   # Gri-albastru pentru strazi
STREET_BORDER = (100, 100, 150)
CAR_COLOR = (255, 50, 50)     # Rosu aprins
TEXT_COLOR = (255, 255, 255)
PADDING = 50                  # Cati pixeli lasam liberi pe margine

# Variabile globale pentru limitele hartii
MIN_LAT, MAX_LAT = 0, 0
MIN_LON, MAX_LON = 0, 0

def load_map_bounds(streets):
    """
    Scaneaza toate strazile ca sa gaseasca marginile extreme ale hartii.
    Asta ne ajuta sa facem 'Auto Zoom' ca sa incapa totul pe ecran.
    """
    global MIN_LAT, MAX_LAT, MIN_LON, MAX_LON
    
    if not streets:
        return

    # Luam toate coordonatele posibile din toate strazile
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
    """
    Transforma coordonate GPS (ex: 47.15) in Pixeli (ex: 500 px).
    """
    # Evitam impartirea la zero daca harta e un singur punct
    if MAX_LON == MIN_LON or MAX_LAT == MIN_LAT:
        return WIDTH // 2, HEIGHT // 2

    # Zona utila de desenare (scadem marginile)
    draw_width = WIDTH - (2 * PADDING)
    draw_height = HEIGHT - (2 * PADDING)

    # 1. Normalizare (0.0 la 1.0)
    # Cat de departe suntem de marginea stanga/jos
    norm_x = (lon - MIN_LON) / (MAX_LON - MIN_LON)
    norm_y = (lat - MIN_LAT) / (MAX_LAT - MIN_LAT)
    
    # 2. Scalare la pixeli
    screen_x = PADDING + int(norm_x * draw_width)
    # ATENTIE: In grafica, Y creste in jos. Pe harta, Lat creste in sus. Inversam Y.
    screen_y = HEIGHT - PADDING - int(norm_y * draw_height)
    
    return screen_x, screen_y

def get_data_from_json(filename):
    """Citeste fisierul JSON in siguranta."""
    try:
        with open(filename, 'r') as f:
            data = json.load(f)
            return data
    except json.JSONDecodeError:
        return [] # Fisierul e gol sau corupt momentan
    except FileNotFoundError:
        return [] # Fisierul nu exista inca
    except Exception as e:
        # print(f"Eroare citire {filename}: {e}")
        return []

# --- MAIN ---
pygame.init()
screen = pygame.display.set_mode((WIDTH, HEIGHT))
pygame.display.set_caption("Traffic Monitor - Live Map")
font = pygame.font.SysFont("Verdana", 12, bold=True)
title_font = pygame.font.SysFont("Verdana", 24, bold=True)

# 1. Incarcam strazile O SINGURA DATA la start
streets = get_data_from_json('streets.json')
if not streets:
    print("EROARE: Nu am gasit streets.json! Asigura-te ca serverul a pornit.")
    # Punem niste date fake doar ca sa nu crape fereastra daca lipseste fisierul
    streets = [] 

load_map_bounds(streets)

running = True
clock = pygame.time.Clock()

while running:
    # --- EVENT HANDLING ---
    for event in pygame.event.get():
        if event.type == pygame.QUIT:
            running = False

    # --- DESENARE ---
    screen.fill(BG_COLOR)

    # Titlu
    title = title_font.render("Sistem Monitorizare Trafic", True, (150, 150, 170))
    screen.blit(title, (20, 20))

    # A. Desenam STRAZILE
    for s in streets:
        # Calculam colturile dreptunghiului strazii
        # Un colt e (Lat_Max, Lon_Min) -> Stanga Sus
        # Un colt e (Lat_Min, Lon_Max) -> Dreapta Jos
        x1, y1 = to_screen(s['lat_max'], s['lon_min'])
        x2, y2 = to_screen(s['lat_min'], s['lon_max'])
        
        # Calculam latimea si inaltimea in pixeli
        rect_w = abs(x2 - x1)
        rect_h = abs(y2 - y1)
        
        # Facem strazile vizibile chiar daca sunt foarte subtiri matematic
        rect_w = max(rect_w, 10) 
        rect_h = max(rect_h, 10)
        
        # Desenam corpul strazii
        top_left_x = min(x1, x2)
        top_left_y = min(y1, y2)
        
        pygame.draw.rect(screen, STREET_COLOR, (top_left_x, top_left_y, rect_w, rect_h))
        pygame.draw.rect(screen, STREET_BORDER, (top_left_x, top_left_y, rect_w, rect_h), 1) # Contur
        
        # Numele strazii (centrat)
        text = font.render(s['name'], True, (200, 200, 200))
        text_rect = text.get_rect(center=(top_left_x + rect_w/2, top_left_y + rect_h/2))
        screen.blit(text, text_rect)

    # B. Desenam MASINILE (Live)
    cars = get_data_from_json('cars.json')
    
    for car in cars:
        # Coordonate
        cx, cy = to_screen(car['lat'], car['lng'])
        
        # Desenam masina (Cerc cu bordura)
        pygame.draw.circle(screen, CAR_COLOR, (cx, cy), 10)
        pygame.draw.circle(screen, (255, 255, 255), (cx, cy), 12, 2)
        
        # Info text (ID si Viteza)
        info_text = f"{car['id']} | {car['speed']}km/h"
        text_surf = font.render(info_text, True, TEXT_COLOR)
        
        # Desenam textul putin mai sus de masina
        screen.blit(text_surf, (cx - 20, cy - 25))

    # --- UPDATE ECRAN ---
    pygame.display.flip()
    
    # 60 FPS (dar fisierul se va citi cat de repede poate bucla, poti pune un time.sleep mic daca vrei)
    clock.tick(30) 

pygame.quit()
sys.exit()