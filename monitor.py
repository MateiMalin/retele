import pygame
import json
import time
import sys
import math

# --- CONFIGURARE VIZUALA (THEME: CYBERPUNK ULTIMATE) ---
WIDTH, HEIGHT = 1000, 800

# Culori
C_BG          = (5, 10, 20)       # Aproape negru
C_GRID        = (20, 30, 40)
C_STREET_FILL = (0, 255, 255)     # Baza pt strazi (folosim Alpha dinamic)
C_STREET_OUT  = (0, 100, 150)     # Bordura
C_CAR         = (255, 50, 50)     # Rosu
C_TRAIL       = (255, 100, 100)   # Urma masinii
C_RADAR       = (0, 255, 100)     # Verde Radar
C_TEXT        = (200, 220, 255)
C_HOVER       = (255, 255, 255)   # Culoare cand pui mouse-ul

PADDING = 60

# --- STATE MEMORY ---
# Aici tinem minte unde ERA masina, ca sa o miscam fin spre unde TREBUIE sa fie
# Structura: id -> { 'x': float, 'y': float, 'trail': [(x,y), (x,y)...] }
car_states = {} 

# Variabile globale harta
MIN_LAT, MAX_LAT = 0, 0
MIN_LON, MAX_LON = 0, 0

def load_map_bounds(streets):
    global MIN_LAT, MAX_LAT, MIN_LON, MAX_LON
    if not streets: return
    
    lats, lons = [], []
    for s in streets:
        lats.extend([s.get('lat_min', 0), s.get('lat_max', 100)])
        lons.extend([s.get('lon_min', 0), s.get('lon_max', 100)])
    
    MIN_LAT, MAX_LAT = min(lats), max(lats)
    MIN_LON, MAX_LON = min(lons), max(lons)
    print(f"[SYSTEM] Harta Calibrata.")

def to_screen(lat, lon):
    if MAX_LON == MIN_LON or MAX_LAT == MIN_LAT: return WIDTH//2, HEIGHT//2
    
    draw_w = WIDTH - (2 * PADDING)
    draw_h = HEIGHT - (2 * PADDING) - 40

    nx = (float(lon) - MIN_LON) / (MAX_LON - MIN_LON)
    ny = (float(lat) - MIN_LAT) / (MAX_LAT - MIN_LAT)
    
    sx = PADDING + int(nx * draw_w)
    sy = (HEIGHT - PADDING - 40) - int(ny * draw_h)
    return sx, sy

def get_json(filename):
    try:
        with open(filename, 'r') as f: return json.load(f)
    except: return []

# --- EFECTE SPECIALE ---

def draw_radar_sweep(screen, angle):
    """ Deseneaza o linie care se invarte ca un radar """
    # Centrul radarului (mijlocul ecranului)
    cx, cy = WIDTH // 2, HEIGHT // 2
    length = 600 # Cat de lunga e raza
    
    # Calculam capatul liniei folosind trigonometrie
    end_x = cx + math.cos(math.radians(angle)) * length
    end_y = cy + math.sin(math.radians(angle)) * length
    
    # Desenam linia cu fade-out (folosim o suprafata transparenta)
    radar_surf = pygame.Surface((WIDTH, HEIGHT), pygame.SRCALPHA)
    
    # Desenam mai multe linii pentru un efect de "coada" la radar
    for i in range(20):
        alpha = 100 - (i * 5)
        if alpha < 0: alpha = 0
        lag_angle = angle - i
        lx = cx + math.cos(math.radians(lag_angle)) * length
        ly = cy + math.sin(math.radians(lag_angle)) * length
        pygame.draw.line(radar_surf, (*C_RADAR, alpha), (cx, cy), (lx, ly), 2)

    screen.blit(radar_surf, (0,0))

def draw_trails(screen):
    """ Deseneaza cozile luminoase ale masinilor """
    trail_surf = pygame.Surface((WIDTH, HEIGHT), pygame.SRCALPHA)
    
    for cid, state in car_states.items():
        points = state['trail']
        if len(points) < 2: continue
        
        # Desenam linii intre punctele din istoric
        for i in range(len(points) - 1):
            # Alpha creste spre capat (coada se estompeaza in spate)
            alpha = int(255 * (i / len(points)))
            start_p = points[i]
            end_p = points[i+1]
            pygame.draw.line(trail_surf, (*C_TRAIL, alpha), start_p, end_p, 3)
            
    screen.blit(trail_surf, (0,0))

def update_smooth_movement():
    """ Interpolare: Misca pozitia curenta (state) spre tinta (server) """
    cars = get_json('cars.json')
    
    # Set de ID-uri active pentru a sterge masinile care au iesit
    active_ids = set()

    for car in cars:
        cid = car.get('id')
        if not cid: continue
        
        # Coordonate Tinta (unde zice serverul ca e)
        lat = car.get('lat')
        lon = car.get('long', car.get('lng', car.get('lon')))
        if lat is None or lon is None: continue
        
        tx, ty = to_screen(lat, lon)
        active_ids.add(cid)
        
        # Daca e masina noua, o adaugam direct
        if cid not in car_states:
            car_states[cid] = {'x': tx, 'y': ty, 'trail': [], 'data': car}
        else:
            # Daca exista, actualizam doar tinta si datele (viteza etc)
            car_states[cid]['target_x'] = tx
            car_states[cid]['target_y'] = ty
            car_states[cid]['data'] = car

    # Fizica de miscare (LERP - Linear Interpolation)
    for cid, state in car_states.items():
        if 'target_x' in state:
            # Ne miscam cu 10% din distanta ramasa la fiecare frame
            # Asta creeaza efectul de incetinire lina la sosire
            state['x'] += (state['target_x'] - state['x']) * 0.1
            state['y'] += (state['target_y'] - state['y']) * 0.1
            
            # Adaugam pozitia curenta in Trail (istoric)
            state['trail'].append((state['x'], state['y']))
            if len(state['trail']) > 15: # Pastram doar ultimele 15 pozitii
                state['trail'].pop(0)

    # Stergem masinile vechi
    to_remove = [k for k in car_states if k not in active_ids]
    for k in to_remove: del car_states[k]

def main():
    pygame.init()
    screen = pygame.display.set_mode((WIDTH, HEIGHT))
    pygame.display.set_caption("TRAFFIC ULTIMATE // NEON EDITION")
    clock = pygame.time.Clock()
    
    font = pygame.font.SysFont("Verdana", 10, bold=True)
    font_large = pygame.font.SysFont("Verdana", 14, bold=True)

    streets = get_json('streets.json')
    load_map_bounds(streets)

    radar_angle = 0
    running = True

    while running:
        # 1. Input
        mx, my = pygame.mouse.get_pos() # Pozitia mouse-ului
        for event in pygame.event.get():
            if event.type == pygame.QUIT: running = False

        # 2. Logic Updates
        update_smooth_movement()
        radar_angle = (radar_angle + 2) % 360 # Rotim radarul
        
        # Efect de pulsare strazi (respiratie)
        street_alpha = int(30 + math.sin(time.time() * 2) * 10)

        # 3. Draw Background
        screen.fill(C_BG)
        
        # Grila fina
        for x in range(0, WIDTH, 50): pygame.draw.line(screen, C_GRID, (x,0), (x,HEIGHT), 1)
        for y in range(0, HEIGHT, 50): pygame.draw.line(screen, C_GRID, (0,y), (WIDTH,y), 1)

        # 4. Draw Streets (Transparent)
        s_surf = pygame.Surface((WIDTH, HEIGHT), pygame.SRCALPHA)
        for s in streets:
            x1, y1 = to_screen(s['lat_max'], s['lon_min'])
            x2, y2 = to_screen(s['lat_min'], s['lon_max'])
            w, h = abs(x2-x1), abs(y2-y1)
            rx, ry = min(x1,x2), min(y1,y2)
            
            # Umplere transparenta pulsatorie
            pygame.draw.rect(s_surf, (*C_STREET_FILL, street_alpha), (rx, ry, w, h))
            # Bordura
            pygame.draw.rect(s_surf, (*C_STREET_OUT, 150), (rx, ry, w, h), 2)
            
            # Nume strada (daca e loc)
            if w > 40 and h > 20:
                txt = font.render(s['name'][:10], True, (*C_STREET_OUT, 200))
                s_surf.blit(txt, (rx+5, ry+5))
                
        screen.blit(s_surf, (0,0))

        # 5. Draw Radar Sweep
        draw_radar_sweep(screen, radar_angle)

        # 6. Draw Trails (Cozi)
        draw_trails(screen)

        # 7. Draw Cars & Interaction
        hovered_car_text = None
        
        for cid, state in car_states.items():
            cx, cy = int(state['x']), int(state['y'])
            
            # Verificam daca mouse-ul e aproape (Hover Effect)
            dist = math.hypot(cx - mx, cy - my)
            is_hovered = dist < 20
            
            color = C_HOVER if is_hovered else C_CAR
            radius = 10 if is_hovered else 6
            
            # Desenam masina
            pygame.draw.circle(screen, color, (cx, cy), radius)
            # Miez alb
            pygame.draw.circle(screen, (255,255,255), (cx, cy), radius // 2)

            # Glow
            if not is_hovered:
                glow = pygame.Surface((30, 30), pygame.SRCALPHA)
                pygame.draw.circle(glow, (*C_CAR, 50), (15,15), 12)
                screen.blit(glow, (cx-15, cy-15))
            
            # Daca e hover, pregatim textul detaliat
            if is_hovered:
                data = state['data']
                spd = data.get('speed', 0)
                street = data.get('street', 'Unknown')
                hovered_car_text = [
                    f"ID: {cid}",
                    f"SPD: {spd} km/h",
                    f"LOC: {street}"
                ]
                # Linie de la mouse la masina
                pygame.draw.line(screen, C_HOVER, (mx, my), (cx, cy), 1)

        # 8. Draw UI (HUD)
        # Panou jos
        pygame.draw.rect(screen, (10, 15, 25), (0, HEIGHT-40, WIDTH, 40))
        pygame.draw.line(screen, C_STREET_OUT, (0, HEIGHT-40), (WIDTH, HEIGHT-40), 2)
        
        status = f"SYSTEM ONLINE | UNITS: {len(car_states)} | RADAR: {int(radar_angle)}°"
        screen.blit(font_large.render(status, True, C_TEXT), (20, HEIGHT-30))

        # Tooltip-ul de la mouse (daca exista)
        if hovered_car_text:
            bx, by = mx + 15, my + 15
            # Cutie neagra pentru text
            pygame.draw.rect(screen, (0,0,0), (bx, by, 150, 60))
            pygame.draw.rect(screen, C_HOVER, (bx, by, 150, 60), 1)
            
            for i, line in enumerate(hovered_car_text):
                t = font.render(line, True, C_HOVER)
                screen.blit(t, (bx + 5, by + 5 + (i*15)))

        pygame.display.flip()
        clock.tick(60) # 60 FPS pentru animatii fine!

    pygame.quit()
    sys.exit()

if __name__ == "__main__":
    main()