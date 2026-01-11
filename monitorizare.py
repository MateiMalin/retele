# hai sa vad cum naiba functioneaza python
#in idee, eu trebuie sa ma folosesc de json-uri si sa generez datele din ele
#cam atat

#data ingestion
#coordinate transformation
#rendering -> a loop where i draw the things

import pygame #libraria grafia
import json 
import sys #pentru iesirea din program

WIDTH, HEIGHT = 1020, 720


WIDTH, HEIGHT = 1000, 800
BACKGROUND_COLOR = (20, 20, 30)       
STREET_COLOR = (60, 60, 80)   
STREET_BORDER = (100, 100, 150)
CAR_COLOR = (255, 50, 50)     
TEXT_COLOR = (255, 255, 255)
PADDING = 50       

#de astea am nevoie pentru bounds pentru harta
MIN_LAT, MAX_LAT = 0, 0
MIN_LON, MAX_LON = 0, 0
PADDING=50

        

#data o sa fie de type dict adica fix {'field_1':'value_1'}
def get_data_from_json(filename):
    try: 
        with open(filename,'r') as f:
            data=json.load(f)
            return data
    except json.JSONDecodeError:
        return []
    except FileNotFoundError:
        return []
    except Exception as e:
        return []
    
#hai sa facem functia care imi da numarul de pixeli a hartii, nu sa fie totul harcodat

streets = get_data_from_json("streets.json")
def load_map_bounds(streets):
    #pun aici global ca sa ii zic ca vreau sa modific variabilele care sunt globale
    global MIN_LAT, MAX_LAT, MIN_LON, MAX_LON

    #aici trebuie sa iau informatiile din streets.json
    all_lats= []
    all_lons= []
    #astea sunt liste

    for s in streets:
        all_lats.append(s['lat_min'])
        all_lats.append(s['lat_max'])
        all_lons.append(s['lon_min'])
        all_lons.append(s['lon_max'])
    #append adauga un element in lista

    MIN_LAT=min(all_lats)
    MAX_LAT=max(all_lats)
    MIN_LON=min(all_lons)
    MAX_LON=max(all_lons)
load_map_bounds(streets)


def to_screen(lat, lon):
    #convert the map coord to pixels
    if MAX_LAT==MIN_LAT or MAX_LON==MIN_LON:
        return WIDTH // 2, HEIGHT//2
    
    norm_x =(lon -MIN_LON)/(MAX_LON - MIN_LON)
    norm_y = (lat - MIN_LAT) / (MAX_LAT - MIN_LAT)

    draw_width = WIDTH - 2* PADDING
    draw_height= HEIGHT- 2* PADDING

    screen_x=PADDING + int(norm_x * draw_width)
    screen_y=HEIGHT - PADDING - int(norm_y * draw_height)

    return screen_x, screen_y
#acum eu pot sa am coordonatele in pixeli pentru 1 punct x,y de pe harta
#o sa o folosesc ca sa dau draw la fiecare strada in parte

def generate_streets(screen,streets,font):
    #aici o sa generez toate strazile
    for street in streets:
        x1,y1 = to_screen(street['lat_max'], street['lon_min'])
        x2,y2=to_screen(street['lat_min'], street['lon_max'])

        rect_width = max(abs(x2 - x1), 10)
        rect_height = max(abs(y2 - y1), 10)

        top_left_x=min(x1,x2)
        top_left_y=min(y1,y2)

        pygame.draw.rect(screen, STREET_COLOR, (top_left_x, top_left_y, rect_width, rect_height))
        pygame.draw.rect(screen, STREET_BORDER, (top_left_x, top_left_y, rect_width, rect_height), 1)

        #true e pentru anti alliasing
        #true imi creeaza un surface din font, ca sa ii pot da draw
        text = font.render(street['name'], True, TEXT_COLOR)
        text_rect = text.get_rect(center=(top_left_x + rect_width/2, top_left_y + rect_height/2))
        screen.blit(text, text_rect)

def draw_cars(screen, cars, font):
    for car in cars:
        cx, cy = to_screen(car['lat'], car['lng'])
        pygame.draw.circle(screen, CAR_COLOR, (cx, cy), 10)
        pygame.draw.circle(screen, (255, 255, 255), (cx, cy), 12, 2)

        info_text = f"{car['id']} | {car['speed']}km/h"
        text_surf = font.render(info_text, True, TEXT_COLOR)
        screen.blit(text_surf, (cx - 20, cy - 25))


pygame.init()
screen= pygame.display.set_mode((WIDTH,HEIGHT))
font = pygame.font.SysFont("Verdana", 12, bold=True)
title_font = pygame.font.SysFont("Verdana", 24, bold=True)
pygame.display.set_caption('Monitorizare trafic Live')


title_rect=pygame.Rect(100,100,200,120)#this returns a rect type
#primele 2 sunt de la cat incepe forma

clock=pygame.time.Clock()# ca sa dam limit la fps-uri, ca sa nu ia foc procesorul

def main():
    running =True
    while running:
        for event in pygame.event.get():
            if event.type==pygame.QUIT:
                running=False
            #daca avem event de tip key down(am apasat pe ceva) si daca ala e escape
            elif event.type==pygame.KEYDOWN:
                if event.key==pygame.K_ESCAPE:
                    running=False

        screen.fill(BACKGROUND_COLOR)

        title = title_font.render("Sistem Monitorizare Trafic", True, (150, 150, 170))
        screen.blit(title, (55, 20))

        generate_streets(screen,streets, font)

        cars = get_data_from_json("cars.json")
        draw_cars(screen, cars, font)
        
        pygame.display.flip()#da update la tot ecranul
        #adica, dupa ce desenam toate lucrurile alea, o sa ii dam updapte la ecran

    #dont allow the loop to run faster than 60 times per second
        clock.tick(60)
    
    pygame.quit()
    sys.exit()

#asta e, daca vreau sa importez, sa nu dea run main direct
if __name__ == "__main__":
    main()