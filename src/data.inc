; ----------------------------------------------------------------------------
;  data.inc — константы, спрайты-карты, строки, переменные, большие буферы
; ----------------------------------------------------------------------------
section .rdata

align 16, db 0
f_signmask:  dd 0x80000000, 0, 0, 0
flash_col:   dd 0.82, 0.94, 1.0, 0.0            ; B,G,R — тёплый белый
aura_col:    dd 26.0, 24.0, 24.0, 0.0
snow_off:    dw 10, 3, -3, 0, 0, 0, 0, 0
ice_off:     dw 14, 6, 0, 0, 0, 0, 0, 0
trk_mul:     dw 44, 58, 72, 0, 0, 0, 0, 0
sparkle_add: dw 14, 14, 14, 0, 0, 0, 0, 0
wood_base:   dw 40, 72, 112, 0, 0, 0, 0, 0
wood_var:    dw 3, 5, 7, 0, 0, 0, 0, 0
w256:        times 8 dw 256
w255:        times 8 dw 255
gray_w:      dw 1, 2, 1, 0, 0, 0, 0, 0
blood_col:   dw 22, 14, 110, 0, 0, 0, 0, 0
ones16:      times 16 db 1
c_log:       dw 46, 80, 124, 0
c_logend:    dw 100, 150, 196, 0
c_fence:     dw 96, 108, 120, 0
c_stove:     dw 212, 218, 226, 0
align 16, db 0
; тени луны: развёртка по 4 ячейки, выборка 2x2
d128:        times 4 dd 128
d_7f:        times 4 dd 0x7F                    ; высота без бита «под крышей»
v_64:        times 4 dd 64.0                    ; смещение горизонта в ячейке
v_2_5:       times 4 dd 2.5                     ; наименьшая полутень, px
v_40:        times 4 dd 40.0                    ; наибольшая
v_4096:      times 4 dd 4096.0
v_255:       times 4 dd 255.0
v_mflr:      times 4 dd -24.0                   ; −MFLR: «пол» луча над пустым снегом
f_absmask:   dd 0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF
v_0_7:       dd 0.7, 0.7, 0.7, 0.7
v_1_4:       dd 1.4, 1.4, 1.4, 0.0
col_moon:    dd 26.0, 21.0, 16.0, 0.0           ; лунный свет, B,G,R
col_dawn:    dd 50.0, 70.0, 110.0, 0.0
tv_hues:     dd 1.0, 0.85, 0.65, 0.0
             dd 0.7, 0.8, 1.0, 0.0
             dd 1.0, 1.0, 1.0, 0.0
tv_pane_col: dd 255.0, 240.0, 220.0, 0.0
bl_hot11:    dd 0x001F1F1F, 0x001F1F1F, 0x001F1F1F, 0x001F1F1F   ; «горячие» ×32
w_one:       times 8 dw 1
rgb_mask:    dd 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF
w_div7:      times 8 dw 9363
bl_gain:     dw 14, 14, 14, 0, 14, 14, 14, 0
w_400:       times 8 dw 400
bl_wt:       dw 4, 4, 4, 4, 0, 0, 0, 0
             dw 3, 3, 3, 3, 1, 1, 1, 1
             dw 2, 2, 2, 2, 2, 2, 2, 2
             dw 1, 1, 1, 1, 3, 3, 3, 3
cone_col:    dw 90, 175, 255, 0, 0, 0, 0, 0
col_tv:      dd 170.0, 140.0, 110.0, 0.0

; свет неба по часам 16:00 .. 08:00 (B,G,R,0), 128 = 1.0
amb_tab:
    dd 120.0, 124.0, 128.0, 0.0     ; 16
    dd 104.0,  96.0, 118.0, 0.0     ; 17
    dd  70.0,  52.0,  44.0, 0.0     ; 18
    dd  50.0,  34.0,  28.0, 0.0     ; 19
    dd  44.0,  30.0,  24.0, 0.0     ; 20
    dd  40.0,  27.0,  22.0, 0.0     ; 21
    dd  38.0,  26.0,  21.0, 0.0     ; 22
    dd  38.0,  26.0,  21.0, 0.0     ; 23
    dd  38.0,  26.0,  21.0, 0.0     ; 00
    dd  38.0,  26.0,  21.0, 0.0     ; 01
    dd  38.0,  26.0,  21.0, 0.0     ; 02
    dd  38.0,  26.0,  21.0, 0.0     ; 03
    dd  38.0,  26.0,  21.0, 0.0     ; 04
    dd  40.0,  28.0,  23.0, 0.0     ; 05
    dd  50.0,  36.0,  30.0, 0.0     ; 06
    dd  76.0,  66.0,  66.0, 0.0     ; 07
    dd 108.0, 104.0, 106.0, 0.0     ; 08

col_lamp:    dd 40.0, 105.0, 190.0, 0.0         ; натриевый фонарь
col_window:  dd 40.0, 110.0, 200.0, 0.0
col_room:    dd 40.0, 105.0, 190.0, 0.0

f_0:        dd 0.0
f_1:        dd 1.0
f_2:        dd 2.0
f_3:        dd 3.0
f_4:        dd 4.0
f_5:        dd 5.0
f_6:        dd 6.0
f_7:        dd 7.0
f_8:        dd 8.0
f_9:        dd 9.0
f_10:       dd 10.0
f_12:       dd 12.0
f_14:       dd 14.0
f_15:       dd 15.0
f_16:       dd 16.0
f_17:       dd 17.0
f_24:       dd 24.0
f_40:       dd 40.0
f_46:       dd 46.0
f_100:      dd 100.0
f_255:      dd 255.0
f_256:      dd 256.0
f_320:      dd 320.0
f_512:      dd 512.0
f_1024:     dd 1024.0
f_1000:     dd 1000.0
f_neg99:    dd -99.0
f_0_01:     dd 0.01
f_0_03:     dd 0.03
f_0_035:    dd 0.035
f_0_04:     dd 0.04
f_0_05:     dd 0.05
f_0_055:    dd 0.055
f_0_07:     dd 0.07
f_0_08:     dd 0.08
f_m0_08:    dd -0.08
f_0_1:      dd 0.1
f_m0_1:     dd -0.1
f_0_12:     dd 0.12
f_0_16:     dd 0.07
f_0_2:      dd 0.2
f_0_25:     dd 0.25
f_0_3:      dd 0.3
f_0_33:     dd 0.33
f_0_35:     dd 0.35
f_0_36:     dd 0.36
f_0_45:     dd 0.45
f_0_5:      dd 0.5
f_0_52:     dd 0.52
f_0_6:      dd 0.6
f_0_8:      dd 0.8
f_0_86:     dd 0.86
f_1_1:      dd 1.1
f_1_3:      dd 1.3
f_1_5:      dd 1.5
f_2_2:      dd 2.2
f_2_25:     dd 2.25
f_2_4:      dd 2.4
f_2_5:      dd 2.5
f_2_6:      dd 2.6
f_3_4:      dd 3.4
f_4_5:      dd 4.5
f_5_5:      dd 5.5
f_6_5:      dd 6.5
f_7_5:      dd 5.5
f_12_5:     dd 12.5
f_15_99:    dd 15.99
f_17_4:     dd 17.4
f_31_5:     dd 31.5
f_1em4:     dd 0.0001
f_inv12:    dd 0.083333
f_inv24:    dd 5.9604645e-8
f_inv60:    dd 0.0166667
f_inv230:   dd 0.0043478
f_inv256:   dd 0.00390625
f_inv340:   dd 0.0029412
f_rad:      dd 0.24                     ; радиус тела (≈ 5 px по экрану, как фигура)
f_walk:     dd 2.2
f_run:      dd 3.7
f_flash:    dd 210.0
f_timescale: dd 1.6
f_dawn:     dd 1920.0
f_zwander:  dd 0.42
f_zchase:   dd 1.35
f_zinvest:  dd 1.0
f_inv2pi:   dd 0.15915494
f_0_225:    dd 0.225
f_pi_2:     dd 1.5707964
f_0_707:    dd 0.70710678
f_1_2:      dd 1.2
f_0_4:      dd 0.4
f_0_9:      dd 0.9
f_0_65:     dd 0.65
f_0_7:      dd 0.7
f_0_125:    dd 0.125
f_2_45:     dd 2.45                     ; px высоты на ячейку карты теней (1/8 тайла)
f_0_9999:   dd 0.9999
f_m0_9999:  dd -0.9999
f_inv255:   dd 0.0039215686
f_0_09:     dd 0.09
f_0_28:     dd 0.28
f_0_21:     dd 0.3
f_2_05:     dd 2.05
f_0_22:     dd 0.28
f_1_43:     dd 1.43
f_0_85:     dd 0.85
f_0_965:    dd 0.965
f_0_915:    dd 0.915
f_50:       dd 50.0
f_inv9:     dd 0.11111111
f_1_25:     dd 1.25
f_1_7:      dd 1.7
f_20:       dd 20.0
f_210:      dd 210.0
f_196:      dd 196.0
f_0_88:     dd 0.88
f_m0_3:     dd -0.3
f_m1:       dd -1.0
f_inv163:   dd 0.0061349693
f_1_9:      dd 1.9
f_30:       dd 30.0
f_0_015:    dd 0.015
f_0_06:     dd 0.06
f_19:       dd 19.0
f_0_32:     dd 0.32
f_128:      dd 128.0
f_26:       dd 26.0
f_3_5:      dd 3.5
f_49:       dd 49.0
f_34:       dd 34.0
f_18:       dd 18.0
f_m6:       dd -6.0
f_22:       dd 22.0
f_m26:      dd -26.0
f_70:       dd 70.0
f_0_13:     dd 0.2
f_inv39:    dd 0.025641
f_9_5:      dd 9.5
f_inv512:   dd 0.001953125
f_0_025:    dd 0.025
f_1_4:      dd 1.4
f_1_6:      dd 1.6
f_3_1:      dd 3.1
f_0_75:     dd 0.75
f_0_02:     dd 0.02
f_m0_8:     dd -0.8
f_inv16:    dd 0.0625
f_inv32:    dd 0.03125
f_rog:      dd 48.0                     ; ROG
f_rov:      dd 72.0                     ; ROV
f_rzw:      dd 36.0                     ; RZW
f_rch:      dd 40.0                     ; RCH
f_1e9:      dd 1.0e9
f_m1e9:     dd -1.0e9
f_32:       dd 32.0
f_64:       dd 64.0
f_192:      dd 192.0
f_m40:      dd -40.0
f_56:       dd 56.0
f_61_5:     dd 61.5
f_37_5:     dd 37.5
f_91_5:     dd 91.5
f_0_55:     dd 0.55
f_3_3:      dd 3.3
; рельеф снега по виду тайла: 0 нет, 1 сруб, 2 забор, 3 мелочь, 4 ель, 5 берёза, 6 дорога
dr_kind:    db 0, 6, 0, 1, 1, 1, 2, 4,  5, 3, 0, 0, 3, 1, 0, 3
            db 0, 3, 0, 3, 3, 3, 0, 0
            times 8 db 0
align 4, db 0
dr_h:       dd 0.0, 96.0, 48.0, 36.0    ; высота сугроба, 1/16 px
dr_r:       dd 1.0, 1.2, 0.8, 0.6       ; его протяжённость, тайлы
; квантование наклона (−8..7) -> 0..4 и нормаль рельефа по (qy·5 + qx)
q5:         db 0, 0, 0, 1, 1, 1, 1, 2,  2, 2, 3, 3, 3, 3, 4, 4
gn_lut:
%assign kk 0
%rep 25
  %if kk == 12
    db N_UP
  %elif kk < 12
    db N_G0 + kk
  %else
    db N_G0 + kk - 1
  %endif
  %assign kk kk+1
%endrep
align 4, db 0
; столкновения и удары
f_mapr:     dd 127.76                   ; MAPW − f_rad
f_creach:   dd 0.39                     ; f_rad + вылет фигур за тайл
f_1em6:     dd 0.000001
f_0_001:    dd 0.001
f_inv_mstep: dd 8.3333                  ; 1 / шаг движения 0.12
f_body2:    dd 0.0576                   ; f_rad²
f_zreach2:  dd 0.4624                   ; 0.68² — зомби дотягивается
f_sw_r:     dd 0.85                     ; лопата от центра игрока
f_sw_far2:  dd 1.1881                   ; (f_sw_r + f_rad)²
f_sw_cos:   dd 0.573576                 ; cos 55°
f_sw_sin:   dd 0.819152
f_sw_sinr:  dd 0.196596                 ; sin 55° · f_rad
f_sw_dc:    dd 0.998848                 ; поворот на 110°/SW_N
f_sw_ds:    dd 0.047978
f_0_22s:    dd 0.22                     ; длительность взмаха
f_swn:      dd 40.0                     ; SW_N
f_pxu:      dd 0.0766                   ; 8.8 по земле -> px высоты
f_19_6:     dd 19.6                     ; тайл -> px высоты
; крыши
bayer4:     db 0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5
scal8:      db 0, 1, 2, 2, 2, 2, 1, 0   ; фестоны причелин и подзора
align 4, db 0
gab_pal:    dd 0x3C6AB8, 0x3E7452, 0xA8843E, 0x86402E, 0xC4C8CC, 0x3A8484
ra_elem:    dw -32, 44, 0, 36, 32, 28, 56, 22   ; вибраторы: сдвиг по стреле, полудлина
horse:      ; 12 колонок (бит k — колонка k наружу от фронтона), 16 строк сверху вниз:
            ; уши, голова с глазом и мордой наружу, шея, охлупень на коньке
    dw 0x180, 0x3C0, 0x7C0, 0xEE0, 0xFE0, 0xCE0, 0x8E0, 0x070
    dw 0x070, 0x038, 0x038, 0x01C, 0x01F, 0x00F, 0x00F, 0x007
; ветер
fw_dirw:    dd 0.043
fw_g1w:     dd 0.23
fw_g1a:     dd 0.28
fw_g2w:     dd 0.61
fw_g2p:     dd 1.3
fw_g2a:     dd 0.18
fw_g3w:     dd 1.33
fw_g3p:     dd 0.4
fw_g3a:     dd 0.12
; погода за ночь: события — центр (ч), полуширина (ч), прибавка ветра, снега
wv_def:     dd 21.3, 1.8, 0.55, 0.0     ; к вечеру крепчает, небо ясное — позёмка
            dd 24.9, 2.1, 0.85, 0.83    ; к полуночи метель
            dd 29.8, 1.5, 0.25, 0.3     ; под утро снежок с ветерком
wv_rnd:     dd 0.8, 1.3, 0.8            ; разброс центров в игре, ч
f_wb0:      dd 0.12                     ; ветер без событий
f_sb0:      dd 0.15                     ; снег без событий
f_wslee:    dd 4.7                      ; спад затишья за заслоном, px высоты на тайл
f_wsfront:  dd 16.0                     ; подпор перед ним
f_wel:      dd 0.93866                  ; e^(−1/15.8)
f_wef:      dd 0.92004                  ; e^(−1/12)
f_gp1:      dd 102.4                    ; период шума крупных порывов, тайлы (64 текселя по 1.6)
f_gp2:      dd 35.2                     ; мелких (по 0.55)
f_gk1:      dd 160.0                    ; тексели·256 на тайл
f_gk2:      dd 465.45
f_inv65280: dd 1.5318627e-5
f_wspd:     dd 6.0                      ; тайлов/с при силе 1
f_wsu:      dd 96.0                     ; 16 px на тайл вдоль экрана · f_wspd
f_wsv:      dd 48.0                     ; 8 px поперёк
f_wh_cx:    dd 17.0                     ; стрелка ветра в HUD
f_wh_cy:    dd 45.0
f_0_15:     dd 0.15
f_0_18:     dd 0.18
f_0_42:     dd 0.42
f_0_0075:   dd 0.0075
f_0_018:    dd 0.018
f_m0_5:     dd -0.5
f_m0_6:     dd -0.6
f_m0_075:   dd -0.075
f_1_8:      dd 1.8
f_3_2:      dd 3.2
f_4_84:     dd 4.84
f_5_7:      dd 5.7
f_6_28:     dd 6.2831853
f_8_4:      dd 8.4
f_11:       dd 11.0
f_21:       dd 21.0
f_25:       dd 25.0
f_m18:      dd -18.0
f_80:       dd 80.0
f_90:       dd 90.0
f_120:      dd 120.0
f_140:      dd 140.0
f_153:      dd 153.0
f_205:      dd 205.0
f_360:      dd 360.0
f_500:      dd 500.0
f_640:      dd 640.0
f_1900:     dd 1900.0
f_inv15:    dd 0.0666667
f_inv48:    dd 0.0208333
f_inv70:    dd 0.0142857
f_160:      dd 160.0
f_36:       dd 36.0
f_110:      dd 110.0
f_215:      dd 215.0
f_0_005:    dd 0.005
f_238:      dd 238.0
f_0_5817:   dd 0.5817                   ; cos, sin 0.95 рад — веточки инея
f_0_8134:   dd 0.8134
; проницаемость для ветра по виду тайла, ×128 (избы — по rmap, целиком)
;           снег дор. пол  стена окно окноС забор ель   берёза фонарь печь стол  дрова дверь печьС крест
wob_k:  db  128, 128, 128, 128, 128, 128,  180,  56,   24,    0,     128, 128,  128,  128,  128,   0
        db  100, 90,  10,  120, 0,   90,   0, 0
        times 8 db 0
; румбы: С (+x), СВ, В (+y), ЮВ, Ю, ЮЗ, З, СЗ
rh_tab:     dd 1.0, 0.0,  0.7071, 0.7071,  0.0, 1.0,  -0.7071, 0.7071
            dd -1.0, 0.0,  -0.7071, -0.7071,  0.0, -1.0,  0.7071, -0.7071
align 16, db 0
memtint_f:  dd 60.0, 50.0, 46.0, 0.0    ; «память» (B, G, R)
haze_f:     dd 14.0, 12.0, 11.0, 0.0    ; пелена мглы
w_drift:    times 8 dw 190              ; позёмка: свет·190/64 — летящий снег светлее лежащего
frost_col:  dw 184, 174, 160, 0         ; иней (B, G, R)
; небо: луна восходит 18:36 на востоке (слева-снизу экрана), заходит 07:00 на западе
fs_mrise:   dd 18.6
fs_mset:    dd 31.0
fs_mfade:   dd 30.2
fs_mspan:   dd 0.253354
fs_emax:    dd 0.62
fs_bk:      dd 0.8308
fs_b0:      dd 1.40
fs_bset:    dd 4.01
fs_d0:      dd 17.2
fs_d1:      dd 0.8333
fs_cw:      dd 0.035
fs_ca:      dd 0.55
fs_c0:      dd 0.15
fs_cls:     dd 172.0

; свойства тайлов: бит0 твёрдый, бит1 непрозрачный, биты4-5 вид пола (0 снег, 1 дорога, 2 доски)
tprop:  db 0x00, 0x10, 0x20, 0x23, 0x21, 0x21, 0x01, 0x01
        db 0x01, 0x01, 0x21, 0x21, 0x01, 0x20, 0x21, 0x01
        db 0x01, 0x01, 0x00, 0x01, 0x01, 0x01, 0x00, 0x00
        times 8 db 0
tdraw:  db D_NONE, D_NONE, D_NONE, D_WALL, D_WIN, D_WINL, D_FENCE, D_SPRUCE
        db D_BIRCH, D_LAMP, D_STOVE, D_TABLE, D_PILE, D_NONE, D_STOVEL, D_CROSS
        db D_SNOWMAN, D_BENCH, D_LINE, D_KENNEL, D_BIRDH, D_WELL, D_NONE, D_NONE
        times 8 db 0
; столкновения по виду тайла (забор — по соседям, см. build_cmap)
tcol:   db 0, 0, 0, CS_TALL, CS_TALL, CS_TALL, 0, CS_SPRUCE
        db CS_BIRCH, CS_LAMP, CS_TALL, CS_LOW, CS_LOW, 0, CS_TALL, CS_CROSS
        db CS_SNOWMAN, CS_BENCH, CS_LINE, CS_KENNEL, CS_BIRDH, CS_WELL, 0, 0
        times 8 db 0
; набор фигур: число, затем скруглённые прямоугольники x0, y0, x1, y1, радиус
; (координаты тайла; спрайт стоит основанием в (0.6, 0.6), 1 px по экрану вбок
; = 1/16 по x−y, фигуры сняты с картинок)
%macro CSET 0-10
    dd %0 / 5
  %rep %0
    dd %1
    %rotate 1
  %endrep
    times 11 - %0 dd 0
%endmacro
align 4, db 0
csets:
    CSET                                            ; пусто
    CSET 0.0, 0.0, 1.0, 1.0, 0.0                    ; сруб, окно, печь — держит удар
    CSET 0.0, 0.0, 1.0, 1.0, 0.0                    ; стол, дрова
    CSET 0.0, 1.0, 1.0, 1.0, 0.07                   ; забор по кромке +y
    CSET 1.0, 0.0, 1.0, 1.0, 0.07                   ; по кромке +x
    CSET 0.0, 1.0, 1.0, 1.0, 0.07, 1.0, 0.0, 1.0, 1.0, 0.07
    CSET 0.6, 0.6, 0.6, 0.6, 0.3                    ; ель: нижние лапы
    CSET 0.6, 0.6, 0.6, 0.6, 0.1                    ; берёза: ствол
    CSET 0.6, 0.6, 0.6, 0.6, 0.09                   ; фонарный столб
    CSET 0.6, 0.6, 0.6, 0.6, 0.13                   ; крест с холмиком
    CSET 0.6, 0.6, 0.6, 0.6, 0.2                    ; снеговик
    CSET 0.756, 0.444, 0.756, 0.444, 0.15, 0.444, 0.756, 0.444, 0.756, 0.15   ; лавочка
    CSET 0.131, 1.069, 0.131, 1.069, 0.06, 1.038, 0.163, 1.038, 0.163, 0.06   ; столбы верёвки
    CSET 0.725, 0.475, 0.725, 0.475, 0.2, 0.475, 0.725, 0.475, 0.725, 0.2     ; будка
    CSET 0.6, 0.6, 0.6, 0.6, 0.08                   ; шест скворечника
    CSET 0.1, 0.7, 0.5, 1.1, 0.03, 0.834, 0.366, 0.834, 0.366, 0.08           ; сруб колодца, стояк журавля
align 16, db 0
sw_add:     dw 50, 54, 60, 0, 0, 0, 0, 0    ; иней в следе лопаты
; 32 направления (cos, sin) — контуры хитбоксов (F3)
hb_dirs:
    dd 1.0, 0.0,           0.980785, 0.19509,   0.92388, 0.382683,   0.83147, 0.55557
    dd 0.707107, 0.707107, 0.55557, 0.83147,    0.382683, 0.92388,   0.19509, 0.980785
    dd 0.0, 1.0,           -0.19509, 0.980785,  -0.382683, 0.92388,  -0.55557, 0.83147
    dd -0.707107, 0.707107, -0.83147, 0.55557,  -0.92388, 0.382683,  -0.980785, 0.19509
    dd -1.0, 0.0,          -0.980785, -0.19509, -0.92388, -0.382683, -0.83147, -0.55557
    dd -0.707107, -0.707107, -0.55557, -0.83147, -0.382683, -0.92388, -0.19509, -0.980785
    dd 0.0, -1.0,          0.19509, -0.980785,  0.382683, -0.92388,  0.55557, -0.83147
    dd 0.707107, -0.707107, 0.83147, -0.55557,  0.92388, -0.382683,  0.980785, -0.19509
; под крышей (2): пол, печь, стол, дверь — в карте высот крыша, AO как под крышей
;          снег дор. пол  стена окно окноС забор ель   берёза фонарь печь стол  дрова дверь печьС крест
th_m:   db 0,  0,  2,  0,   0,   0,    0,    0,    0,     1,     2,   2,    0,    2,    2,     0
        db 0,  0,  0,  0,   1,   0,    0, 0
        times 8 db 0
; запечка огней: 4 луча — точки сэмпла (±1/16 тайла), затем точки огня (±0.07), 8.8
align 4, db 0
lv_tab:     dd -16, -16,  16, -16,  -16, 16,  16, 16
            dd 18, 0,  0, 18,  0, -18,  -18, 0
; тени луны (8 ячеек на тайл): спрайт -> тело вращения по силуэту (номер профиля),
; мелочи — фигуры столкновения своей высоты (px: первая, вторая фигура набора)
hm_prof:    db 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0,    1,    2,    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 3
            db 4,    0xFF, 0xFF, 0xFF, 5,    0xFF, 0xFF, 0xFF
            times 8 db 0xFF
hm_csh:     times 17 dw 0
            db 9, 9                     ; лавочка
            db 26, 26                   ; столбы верёвки
            db 15, 15                   ; будка
            db 0, 0
            db 16, 46                   ; сруб колодца, стояк журавля
            times 10 dw 0
pf_spt:     db 0, 1, 2, 4, 5, 9         ; ель, берёза, фонарь, крест, снеговик, скворечник (sprtab)
pf_gain:    db 64, 18, 255, 255, 255, 255   ; x64: густота кроны -> доля заслоняющих ячеек
                                        ; (голая берёза — кружево, ель — почти сплошь)
; AO: вес и радиус (сотые)
ao_w:   db 0,  0,  55, 55,  55,  55,   28,   50,   12,    5,     55,  55,   35,   55,   55,    8
        db 22, 15, 0,  28,  0,   28,   0, 0
        times 8 db 0
ao_r:   db 0,  0,  90, 90,  90,  90,   55,   90,   45,    30,    90,  90,   60,   90,   90,    35
        db 50, 45, 0,  55,  0,   60,   0, 0
        times 8 db 0
; силуэты на подоконнике (строки wh 2..6, бит k — wu 2+k)
cat_rows:   db 0x1F, 0x17, 0x17, 0x07, 0x05
ger_pot:    db 0x18, 0x18, 0, 0, 0
ger_leaf:   db 0, 0, 0x1C, 0x08, 0x08
ger_flow:   db 0, 0, 0, 0x10, 0x10
well_spots: db 41, 56, 87, 66, 33, 68, 95, 55, 64, 40, 60, 86, 0xFF
logshade: db 120, 200, 245, 255, 225, 160
lognrm:   db N_LYU+1, N_LYU+1, N_FY, N_FY, N_LYU, N_LYU, 0, 0             ; нормаль ряда бревна, грань +y
          db N_LXU+1, N_LXU+1, N_FX, N_FX, N_LXU, N_LXU, 0, 0             ; грань +x
; нормали (x, y, вверх) x100, нормируются при старте
align 2, db 0
nraw:
    dw 0, 0, 100            ; 0 вверх
    dw 0, 100, 0            ; 1 грань +y
    dw 100, 0, 0            ; 2 грань +x
    dw 0, 77, 64            ; 3 бревно +y, верхняя половина
    dw 0, 77, -64           ; 4 нижняя
    dw 77, 0, 64            ; 5 бревно +x
    dw 77, 0, -64           ; 6
    dw 26, 97, 38           ; 7 спрайт стоя: левая треть
    dw 71, 71, 38           ; 8 середина
    dw 97, 26, 38           ; 9 правая
    dw 0, 61, 100           ; 10 скат к +y
    dw 0, -61, 100          ; 11 к -y
    dw 61, 0, 100           ; 12 к +x
    dw -61, 0, 100          ; 13 к -x
    dw 0, 85, 100           ; 14 +y круче (заструги)
    dw 0, 38, 100           ; 15 +y положе
    dw 0, -85, 100          ; 16
    dw 0, -38, 100          ; 17
    dw 85, 0, 100           ; 18
    dw 38, 0, 100           ; 19
    dw -85, 0, 100          ; 20
    dw -38, 0, 100          ; 21
    dw 0, 50, 100           ; 22 сугроб, привалившийся к грани +y
    dw 50, 0, 100           ; 23 к грани +x
%assign jj -2
%rep 5
  %assign ii -2
  %rep 5
    %if ii != 0 || jj != 0
    dw ii*18, jj*18, 100    ; 24.. рельеф снега
    %endif
    %assign ii ii+1
  %endrep
  %assign jj jj+1
%endrep
    dw 26, 97, 38           ; 48 мелкие спрайты — те же трети
    dw 71, 71, 38
    dw 97, 26, 38
    dw 26, 97, 38           ; 51 крупные
    dw 71, 71, 38
    dw 97, 26, 38
; на сколько тайлов к луне сдвинуть выборку тени по нормали: у картинок-столбиков
; своя тень не должна ложиться на них самих (фигура, мелочь, ель), у стен — чуть наружу
align 4, db 0
msd:
    dd 0.0, 0.05, 0.05, 0.05, 0.05, 0.05, 0.05     ; земля, грани, брёвна
    dd 0.22, 0.22, 0.22                            ; люди и трубы
    times 12 dd 0.0                                ; скаты
    dd 0.0, 0.0                                    ; сугробы у стен
    times 24 dd 0.0                                ; рельеф снега
    dd 0.42, 0.42, 0.42                            ; мелкие спрайты
    dd 0.7, 0.7, 0.7                               ; крупные
blob_w:   db 3, 5, 6, 5, 3
align 2, db 0
nb4:      dw -1, 1, -MAPW, MAPW

; участки: x0, y0, w, h, сторона калитки (0 юг, 1 север, 2 восток, 3 запад)
lot_table:
%assign xx 0
%rep 7
  %if xx == 0
    %assign lx 6
  %elif xx == 1
    %assign lx 19
  %elif xx == 2
    %assign lx 44
  %elif xx == 3
    %assign lx 57
  %elif xx == 4
    %assign lx 70
  %elif xx == 5
    %assign lx 96
  %else
    %assign lx 109
  %endif
    db lx, 46, 12, 13, 0
    db lx, 64, 12, 13, 1
  %assign xx xx+1
%endrep
%assign yy 0
%rep 5
  %if yy == 0
    %assign ly 12
  %elif yy == 1
    %assign ly 26
  %elif yy == 2
    %assign ly 82
  %elif yy == 3
    %assign ly 96
  %else
    %assign ly 110
  %endif
    db 23, ly, 12, 13, 2
    db 40, ly, 12, 13, 3
    db 77, ly, 12, 13, 2
    db 94, ly, 12, 13, 3
  %assign yy yy+1
%endrep
    db 0xFF

align 4, db 0
zcoats: dd 0x5A3A3A, 0x3A4A5A, 0x6A6A5A, 0x4A3A5A, 0x7A5A3A
        dd 0x2E3E2E, 0x5A2A2A, 0x6A6A70, 0x404550, 0x70604A
zpants: dd 0x2A2A30, 0x3A3530, 0x2E3440, 0x454040

; зомби для снимков: dx, dy, состояние
shot_zoff:
    dd 2.5, -3.0, ST_WANDER
    dd 4.0, -1.5, ST_WANDER
    dd -3.0, -2.0, ST_WANDER
    dd 1.0, 3.5, ST_CORPSE
    dd -1.5, -4.5, ST_WANDER
    dd -99.0
shot_zoff5:
    dd 1.0, 0.1, ST_WANDER
    dd 1.4, -0.9, ST_WANDER
    dd -99.0
shot_zoff8:
    dd 2.4, -3.1, ST_WANDER
    dd 3.9, -3.6, ST_WANDER
    dd 3.6, -5.6, ST_WANDER
    dd -99.0
; проверка нюха: ветер к +x — трое по ветру от игрока, трое против, дальше, чем видят
shot_zoffz:
    dd 8.0, 0.0, ST_WANDER
    dd 11.0, 1.0, ST_WANDER
    dd 14.0, -1.0, ST_WANDER
    dd -8.0, 0.0, ST_WANDER
    dd -11.0, 1.0, ST_WANDER
    dd -14.0, -1.0, ST_WANDER
    dd -99.0

yard_offs: db 2, 2, 1, 2, 2, 1, 3, 2, 2, 3, 1, 3, 3, 1, 0, 2, 2, 0, 99
vk_list: db 'W', 'A', 'S', 'D', 0x10, 0x20, 0x01, 'F', 'R', 0x1B, 'M', 0

bmp_hdr:
    db 'BM'
    dd 54 + SCR_W*SCR_H*4, 0, 54
    dd 40, SCR_W, SCR_H
    dw 1, 32
    dd 0, SCR_W*SCR_H*4, 2835, 2835, 0, 0

; ---------------------------------------------------------------- персонажи
; 16x28: верх 19 строк + ноги 9 строк; смотрят вправо, влево — зеркально
%define SPRW 16
cm_pup:
    R "................"
    R "......hhhh......"
    R ".....hhhhhh....."
    R "....hhhhhhhh...."
    R "....HhhhhhhhH..."
    R "....HHssssss...."
    R "....HHsssesS...."
    R "....HHssssss...."
    R ".....HSssssS...."
    R "......SSSS......"
    R "....cccccccc...."
    R "...cccccccccc..."
    R "...cCccccccCc..."
    R "...cCccccccCc..."
    R "...cCccccccCc..."
    R "..scCccccccCcsL."
    R "..s.cccccccc.s.."
    R "....CCCCCCCC...."
    R "....cccccccc...."
cm_patk:
    R "................"
    R "......hhhh......"
    R ".....hhhhhh....."
    R "....hhhhhhhh...."
    R "....HhhhhhhhH..."
    R "....HHssssss...."
    R "....HHsssesS...."
    R "....HHssssss...."
    R ".....HSssssS...."
    R "......SSSS......"
    R "....cccccccc...."
    R "...cccccccccc.w."
    R "...cCccccccCcsw."
    R "...cCccccccCc.w."
    R "...cCccccccCc.mm"
    R "..scCccccccCc.mM"
    R "..s.cccccccc..M."
    R "....CCCCCCCC...."
    R "....cccccccc...."
cm_zup:
    R "................"
    R "................"
    R ".....aaaaa......"
    R "....aaaaaaa....."
    R "....aagggga....."
    R "....agggggg....."
    R "....aggxggx....."
    R "....agggggg....."
    R ".....gGrGgg....."
    R "......GGGG......"
    R "....cccccccc...."
    R "...ccccccccgggg."
    R "...cCcrcccCgggg."
    R "...cCcccccCc...."
    R "...cCccrccCc...."
    R "...cCcccccCc...."
    R "....cccrcccc...."
    R "....cccccccc...."
    R "....crcccccc...."
cm_lidle:
    R ".....pppppp....."
    R ".....ppp.ppp...."
    R ".....pp...pp...."
    R ".....pp...pp...."
    R ".....pp...pp...."
    R ".....bb...bb...."
    R ".....bb...bb...."
    R ".....bbb..bbb..."
    R ".....BBB..BBB..."
cm_la:
    R ".....pppppp....."
    R "....ppp..ppp...."
    R "....pp....pp...."
    R "...pp......pp..."
    R "...pp......pp..."
    R "..bb........bb.."
    R "..bb........bb.."
    R ".bbb........bbb."
    R ".BBB........BBB."
cm_lb:
    R ".....pppppp....."
    R "......pppp......"
    R "......pppp......"
    R "......pp.pp....."
    R ".....pp..pp....."
    R ".....bb..bb....."
    R ".....bb...bb...."
    R "....bbb...bbb..."
    R "....BBB...BBB..."
%undef SPRW
%define SPRW 24
cm_corpse:
    R "........................"
    R "..........cccccc..aaa..."
    R "..pppppcccccccccc.agga.."
    R ".bppppppcccrcccccgggga.."
    R ".bpppppcccccccccc.ggg..."
    R "..pppp..rcccccRr........"
    R "......rrRr.rRRr........."
    R "........RRrr............"

; ---------------------------------------------------------------- строки UTF-16
align 2, db 0
s_face_mono:  dw __utf16__('Consolas'), 0
s_face_serif: dw __utf16__('Georgia'), 0
s_class:      dw __utf16__('StuzhaWnd'), 0
s_wintitle:   dw __utf16__('Стужа — изометрия на NASM'), 0
s_place:      dw __utf16__('Стужа'), 0
s_night:      dw __utf16__(' · январь'), 0
s_kills:      dw __utf16__('Упокоено: '), 0
s_hp:         dw __utf16__('здоровье'), 0
s_sta:        dw __utf16__('силы'), 0
s_bat:        dw __utf16__('фонарь'), 0
s_batdead:    dw __utf16__('фонарь сел'), 0
s_title:      dw __utf16__('СТУЖА'), 0
s_sub:        dw __utf16__('Зима. Мёртвые бродят по селу. Доживи до рассвета.'), 0
s_ctrl:       dw __utf16__('WASD — идти · Shift — бежать · ЛКМ/Пробел — лопата · F — фонарь · M — звук · F11 — экран'), 0
s_dead:       dw __utf16__('ТЫ ПОГИБ'), 0
s_lasted:     dw __utf16__('Продержался до '), 0
s_dawn:       dw __utf16__('РАССВЕТ'), 0
s_survived:   dw __utf16__('Дожил до утра: '), 0
s_sep_kills:  dw __utf16__(' · упокоено: '), 0
s_again:      dw __utf16__('R — заново · Esc — выход'), 0
s_frame:      dw __utf16__('кадр '), 0
s_ms:         dw __utf16__(' мс'), 0
s_warm:       dw __utf16__('тепло'), 0
s_freeze:     dw __utf16__('замерзаешь'), 0
s_wind:       dw __utf16__('ветер '), 0
s_calm:       dw __utf16__('тихо'), 0
s_mps:        dw __utf16__(' м/с'), 0
s_blizz:      dw __utf16__(' · метель'), 0
s_drift:      dw __utf16__(' · позёмка'), 0
s_snowing:    dw __utf16__(' · снег'), 0
s_rh0:        dw __utf16__('С'), 0
s_rh1:        dw __utf16__('СВ'), 0
s_rh2:        dw __utf16__('В'), 0
s_rh3:        dw __utf16__('ЮВ'), 0
s_rh4:        dw __utf16__('Ю'), 0
s_rh5:        dw __utf16__('ЮЗ'), 0
s_rh6:        dw __utf16__('З'), 0
s_rh7:        dw __utf16__('СЗ'), 0
align 8, db 0
rh_str:       dq s_rh0, s_rh1, s_rh2, s_rh3, s_rh4, s_rh5, s_rh6, s_rh7

; ============================================================================
section .data

align 16, db 0
amb_vec:    dd 38.0, 26.0, 21.0, 0.0
lampf_vec:  dd 1.0, 1.0, 1.0, 1.0
tint_vec:   dw 256, 256, 256, 0, 0, 0, 0, 0
amb_raw:    dd 38.0, 26.0, 21.0, 0.0
moon_vec:   dd 0.0, 0.0, 0.0, 0.0
winf_vec:   dd 0.0, 0.0, 0.0, 0.0
lampb_vec:  dd 0.0, 0.0, 0.0, 0.0
tv_vec:     dd 0.0, 0.0, 0.0, 0.0
np:         times 32 db 0               ; заготовка частицы

; спрайты тайлов: указатель, w|h<<8, якорь x, качание (px*16), режим, просвет, нормали
align 8, db 0
sprtab:
    dq spr_spruce
    dd 32 | (64 << 8), 16, 34, 1, 1, N_SB
    dq spr_birch
    dd 20 | (56 << 8), 10, 52, 1, 1, N_SB
    dq spr_lamp
    dd 12 | (50 << 8), 4, 0, 0, 1, N_SM
    dq spr_lamp                         ; D_STOVEL — блок, не спрайт
    dd 12 | (50 << 8), 4, 0, 0, 0, N_SM
    dq spr_cross
    dd 10 | (20 << 8), 4, 0, 0, 0, N_SM
    dq spr_snowman
    dd 14 | (22 << 8), 7, 0, 0, 0, N_SM
    dq spr_bench
    dd 20 | (12 << 8), 10, 0, 0, 0, N_SM
    dq spr_line
    dd 32 | (26 << 8), 16, 0, 0, 1, N_SM
    dq spr_kennel
    dd 22 | (18 << 8), 11, 0, 0, 0, N_SM
    dq spr_birdh
    dd 8 | (46 << 8), 4, 12, 1, 1, N_SM
    dq spr_well
    dd 40 | (58 << 8), 20, 0, 0, 1, N_SB

align 4, db 0
; ветер
wind_t:     dd 0.0
wind_g:     dd 0.5
wind_dx:    dd 0.707
wind_dy:    dd -0.707
wind_sx:    dd 1.0
wind_sy:    dd 0.0
cloud_ox:   dd 0.0
cloud_oy:   dd 0.0
; погода и поле ветра
wv_ev:      times 12 dd 0.0             ; события этой ночи (из wv_def ± разброс)
wv_h:       dd 0.0                      ; час
wv_th:      dd 0.0                      ; куда дует, рад (0 — к +x, с юга)
wsh_th:     dd 99.0                     ; при каком направлении считано затишье
wnd_base:   dd 0.12                     ; средняя сила по погоде
wnd_snow:   dd 0.15                     ; снегопад 0..1
wnd_gust:   dd 0.25                     ; размах порывов по месту
wnd_wo:     dd 0.0                      ; мгла 0..1
gst_ox:     dd 0.0                      ; сдвиг пятен порывов, тайлы
gst_oy:     dd 0.0
gs2_ox:     dd 0.0
gs2_oy:     dd 0.0
wv_sh:      dd 0.0                      ; wind_vec: затишье и порыв последнего вызова
wv_gf:      dd 1.0
wrng:       dd 0x9E3779B9               ; ГСЧ ветра и красоты
vis_r:      dd 21.0                     ; дальность обзора, тайлы (в мгле меньше)
vis_r2:     dd 441.0
vis_k:      dd 0.33                     ; крутизна края обзора
fl_reach:   dd 17.0                     ; дальность фонарика
cone_k:     dd 1.0                      ; яркость конусов фонарей по снегу в воздухе
pz_f1:      dd 0.0                      ; позёмка: сдвиг полос (тексели), направление ×256
pz_f2:      dd 0.0
pz_s1:      dd 0
pz_s2:      dd 0
pz_dx:      dd 256
pz_dy:      dd 0
pl_wvx:     dd 0.0                      ; ветер у игрока (сила·направление), сила, сглаженная
pl_wvy:     dd 0.0
pl_ws:      dd 0.0
pl_wsm:     dd 0.0
pl_warm:    dd 100.0                    ; тепло 0..100
frost_lvl:  dd 0.0                      ; иней на кадре 0..1
frost_thr:  dd 0                        ; порог маски инея на этот кадр (draw_frost)
mv_wp:      dd 0.0                      ; ветер вдоль хода игрока
z_wvx:      dd 0.0                      ; ветер у зомби
z_wvy:      dd 0.0
z_ws:       dd 0.0
z_scent:    dd 0
flk_n:      dd 0                        ; летящих снежинок
flk_base:   dd 30                       ; их яркость без света
fl_tu:      dd 0.0
fl_tv:      dd 0.0
fl_amp:     dd 0.0
up_kind:    dd 0                        ; update_parts: вид, ветер у частицы, завихрения
up_wx:      dd 0.0
up_wy:      dd 0.0
up_s:       dd 0.0
up_t:       dd 0.0
up_ph:      dd 0.0
up_ax:      dd 0.0
up_ay:      dd 0.0
sp_cx:      dd 0.0                      ; снег с конька
sp_cy:      dd 0.0
sp_wx:      dd 0.0
sp_wy:      dd 0.0
sp_len:     dd 0
sm_s:       dd 0.0                      ; ветер над трубой
dw_wx:      dd 0.0                      ; провод: ветер
dw_wy:      dd 0.0
gs_u0:      dd 0.0                      ; gen_streaks
gs_v0:      dd 0.0
gs_w:       dd 0.0
gs_a:       dd 0.0
gs_m:       dd 0.0
gs_k:       dd 0.0
gs_ph:      dd 0.0
gs_il:      dd 0.0
gs_i:       dd 0.0
gs_vc:      dd 0.0
wh_ux:      dd 0.0                      ; стрелка ветра
wh_uy:      dd 0.0
wh_r:       dd 0.0
wh_tx:      dd 0.0
wh_ty:      dd 0.0
wh_side:    dd 0.0
wh_col:     dd 0
wh_ox:      dd 0
wl_x0:      dd 0.0
wl_y0:      dd 0.0
wl_x1:      dd 0.0
wl_y1:      dd 0.0
fr_x:       dd 0.0                      ; gen_frost: игла, веточка
fr_y:       dd 0.0
fr_nx:      dd 0.0
fr_ny:      dd 0.0
fr_a:       dd 0.0
fr_s:       dd 0.0
fr_dx:      dd 0.0
fr_dy:      dd 0.0
fr_v0:      dd 0.0
fr_v:       dd 0.0
fr_bs:      dd 0.0
fr_bx:      dd 0.0
fr_by:      dd 0.0
fr_px:      dd 0.0
fr_py:      dd 0.0
fr_bv:      dd 0.0
dk_tab:     times 16 db 0               ; заметание следов по затишью
align 16, db 0
memtint_v:  dw 60, 50, 46, 0, 0, 0, 0, 0
haze_v:     times 8 dw 0
align 4, db 0
tmp_a:      dd 0.0
tmp_b:      dd 0.0
tmp_c:      dd 0.0
; небо
sky_dx:     dd 1.0
sky_dy:     dd 0.0
sky_tan:    dd 0.5
sk_h:       dd 0.0
sk_phi:     dd 0.0
sk_i:       dd 0.0
sk_cover:   dd 0.3
cl_ox:      dd 0
cl_oy:      dd 0
sky_b:      dd 0
sky_wrap:   dd 0.1                      ; «обёртка» N·L: луна узкая, зарево широкое
flk_moon:   dd 0
sw_sgn:     dd 1
; тени луны
align 16, db 0
mk_k:       dd 0.0                      ; рост полутени луны на px пути
m8_ox:      dd 0                        ; начало развёртки, ячейки
m8_oy:      dd 0
mo_x:       dd 0                        ; то же в 8.8 от центров ячеек (для light_pass)
mo_y:       dd 0
moon_on:    dd 0
nstamp:     dd 0
lt_h:       dd 0                        ; высота огня, px (запечка)
lt_hf:      dd 0                        ; 1 — луч над картой высот, 0 — по тайлам
; огни
lamp_t:     dd 0.0
brk_t:      dd 0.0
brk_st:     dd 0
brk_lvl:    dd 1.0
brk_x:      dd -1
brk_y:      dd -1
tv_t:       dd 0.0
tv_lvl:     dd 0.6
tv_hue:     dd 0
tv_pane:    dd 0x81FFE8D0
breath_add: dd 0x00404040
bb_lines:   dd 0
; заслонители фонарика
nocc:       dd 0
; развёртка теней луны
align 8, db 0
sw_hb:      dq 0                        ; карта высот в начале окна
sw_P:       dq 0                        ; дорожка ближе к луне
sw_C:       dq 0                        ; текущая
sw_u:       dd 0
sw_ymaj:    dd 0                        ; 1 — вдоль y, столбец поперёк по x (подряд в памяти)
sw_ha:      dd 0                        ; шаг по карте вдоль
sw_oa:      dd 0                        ; по mz8, байты: вдоль, поперёк
sw_op:      dd 0
sw_ca:      dd 0                        ; по облакам (mzg) вдоль, на сэмпл, байты
sw_v0:      dd 0                        ; строки развёртки, которые на окно влияют
sw_v1:      dd 0
; генерация
nl_kind:    dd 0
lamp_line:  dd 0
nlamps:     dd 0
nwires:     dd 0
nsmk:       dd 0
H_tv:       dd 0
H_zt:       dd 0
nhouses:    dd 0
; частицы
p_cur:      dd 0
br_t:       dd 1.0
dp_kind:    dd 0
dp_depth:   dd 0
dp_a:       dd 0
dp_r:       dd 0.0
dc_depth:   dd 0
dw_ax:      dd 0
dw_ay:      dd 0
dw_bx:      dd 0
dw_by:      dd 0
dw_g:       dd 0.0
dw_ph:      dd 0.0
dw_sag:     dd 0.0
dw_sw:      dd 0.0
dw_px:      dd 0.0
dw_py:      dd 0.0
dw_dt:      dd 0.0
align 8, db 0
prof_now:   dq 0                        ; замеры проходов рендера
prof_last:  dq 0
prof_acc:   times 24 dq 0
s_profname: dw __utf16__('prof.bin'), 0
s_zlogname: dw __utf16__('zlog.bin'), 0
s_benchname: dw __utf16__('bench.bin'), 0
dbg_bench:  dd 0                        ; --bench: замер кадров в живом окне -> bench.bin
dbg_intro:  dd 0                        ; --intro: заставка поверх снимка
align 8, db 0
fb_dc:      dq 0, 0                     ; два буфера кадра: DC и биты
fb_bits:    dq 0, 0
fb_cur:     dd 0                        ; в какой рисуем сейчас (memdc/fbbits — его)
pr_buf:     dd 0                        ; какой ушёл в поток вывода
pr_on:      dd 0                        ; есть поток вывода (живая игра, --bench)
pr_nodwm:   dd 0                        ; --bench: вывод не ждёт экран
ev_ready:   dq 0                        ; кадр отдан потоку вывода
ev_done:    dq 0                        ; поток вывода отпустил буфер
pr_t0:      dq 0
pr_t1:      dq 0
pr_ticks:   dq 0                        ; сколько поток вывода делал прошлый кадр
tq_n:       dd 0, 0                     ; команд надписей в списке буфера
sky_t0:     dq 0                        ; фоновая развёртка теней: начало, конец,
sky_t1:     dq 0
sky_ticks:  dq 0                        ; сколько шла (для --bench)
align 8, db 0
bench_n:    dd 0                        ; заголовок bench.bin: кадров, слотов, частота QPC
            dd BSLOTS
bench_qpf:  dq 0

align 8, db 0
shot_ztab:  dq shot_zoff
los_map:    dq opq                      ; карта непрозрачности для лучей
hinst:      dq 0
hwnd:       dq 0
hdc:        dq 0
memdc:      dq 0
fbbits:     dq 0
font_s:     dq 0
font_m:     dq 0
font_b:     dq 0
qpf:        dq 0
qpc_last:   dq 0
qpc_now:    dq 0
qpc_end:    dq 0
written:    dq 0
msg:        times 48 db 0
pt:         dd 0, 0
wc:         dd 80, 0x23
            dq 0
            dd 0, 0
            dq 0, 0, 0, 0, 0, 0, 0
wrect:      dd 0, 0, WIN_W, WIN_H
bmi:        dd 40, SCR_W, -SCR_H
            dw 1, 32
            dd 0, 0, 0, 0, 0, 0
            dd 0

keys:
k_w:        db 0
k_a:        db 0
k_s:        db 0
k_d:        db 0
k_shift:    db 0
k_space:    db 0
k_lmb:      db 0
k_f:        db 0
k_r:        db 0
k_esc:      db 0
k_m:        db 0
k_f_prev:   db 0
k_r_prev:   db 0
k_f_edge:   db 0
k_r_edge:   db 0
k_esc_prev: db 0
k_esc_edge: db 0
k_m_prev:   db 0
k_m_edge:   db 0
align 4, db 0
mx:         dd 320
my:         dd 150
shotmode:   dd 0
shot_frames: dd 0
rng:        dd 1
dt:         dd 0.016
gstate:     dd 0
gtime:      dd 1040.0
elapsed:    dd 0.0
kills:      dd 0
spawn_t:    dd 4.0
cam_xf:     dd 64.5
cam_yf:     dd 61.5
camx:       dd 0
camy:       dd 0
cam_a:      dd 0
cam_b:      dd 0
gox:        dd 0
goy:        dd 0
plw_x:      dd 0
plw_y:      dd 0
pdiag:      dd 0
psx:        dd 320
psy:        dd 196
pl_hp:      dd 100.0
pl_sta:     dd 100.0
pl_bat:     dd 100.0
pl_cd:      dd 0.0
pl_swing:   dd 0.0
pl_hurt:    dd 0.0
pl_flash:   dd 0
pl_moving:  dd 0
pl_sprint:  dd 0
mv_x:       dd 0.0
mv_y:       dd 0.0
mv_step:    dd 0.0
noise_r:    dd 0.0
noise_x:    dd 0.0
noise_y:    dd 0.0
see2:       dd 49.0
flash_i:    dd 0.0
glint:      dd 0
grain_off:  dd 0
frame_no:   dd 0
frame_ms:   dd 0.0
tmp_frac:   dd 0
decay_pos:  dd 0
z_pd2:      dd 0.0
z_step:     dd 0.0
z_ox:       dd 0.0
z_oy:       dd 0.0
sep_x:      dd 0.0
sep_y:      dd 0.0
sp_y:       dd 0
bs_x:       dd 0.0
bs_y:       dd 0.0
bs_s:       dd 0.0
lt_x:       dd 0
lt_y:       dd 0
nlights:    dd 0
L_x0:       dd 0
L_y0:       dd 0
L_w:        dd 0
L_h:        dd 0
L_side:     dd 0
H_x:        dd 0
H_y:        dd 0
H_w:        dd 0
H_h:        dd 0
H_lit:      dd 0
mv_dx:      dd 0.0
mv_dy:      dd 0.0
cp_R:       dd 0.0
sw_vx:      dd 0.0
sw_vy:      dd 0.0
sw_s0:      dd 0
; окно: клиентская область и прямоугольник кадра в ней (вписан с полями)
fullscr:    dd 0
start_fs:   dd 0                        ; --fs
dbg_hit:    dd 0                        ; F3 / --hb: контуры хитбоксов
dbg_lmap:   dd 0                        ; --lm: запечённый свет огней в снимке
dbg_wind:   dd 0                        ; --wind: поле ветра в снимке
dbg_soak:   dd 0                        ; --soak: 3000 кадров игры перед снимком
hb_px:      dd 0                        ; предыдущая точка контура
hb_py:      dd 0
hb_shp:     dd 0.0, 0.0, 0.0, 0.0, 0.0  ; временная фигура (тело)
cli_w:      dd WIN_W
cli_h:      dd WIN_H
vw_x:       dd 0
vw_y:       dd 0
vw_w:       dd WIN_W
vw_h:       dd WIN_H
wplace:     dd 44                       ; WINDOWPLACEMENT — где было окно до полного экрана
            times 40 db 0
minfo:      dd 40                       ; MONITORINFO
            times 36 db 0
align 16, db 0
pl:         times ESZ db 0
s_shotname: dw __utf16__('shot?.bmp'), 0
align 2, db 0
tbuf:       times 128 dw 0

; ============================================================================
section .bss
alignb 64
bench_buf:  resq NBENCH*BSLOTS          ; --bench: время каждого прохода каждого кадра
tq_buf:     resb 2*TQMAX*TQSZ           ; списки надписей двух буферов
albedo:     resd SCR_W*SCR_H
gposb:      resd SCR_W*SCR_H
lgrid:      resd GW*GW
slight:     resd SLW*SLW
tracks:     resb DECW*DECW
blood:      resb DECW*DECW
vign:       resb SCR_W*SCR_H
grain:      resb 65536
tmap:       resb MAPW*MAPW
opq:        resb MAPW*MAPW
opq_df:     resb MAPW*MAPW              ; до ближайшего непрозрачного, тайлы (build_opq_df)
alignb 8
los_rcp:    resq LOSN                   ; ⌈2^32 / n⌉ — шаг луча без деления
opqw:       resb MAPW*MAPW              ; то же, окна непрозрачны (свет комнаты)
cmap:       resb MAPW*MAPW              ; набор фигур столкновения (CS_*)
losg:       resb GW*GW
alignb 16
wtab:       resb 64*16
ents:       resb MAXE*ESZ
flakes:     resb NFLK*FL_SZ
lights:     resb MAXL*L_SZ
spr_spruce: resd 32*64
spr_birch:  resd 20*56
spr_lamp:   resd 12*50
spr_cross:  resd 10*20
logtab:     resb 64
alignb 4
lut_log:    resd 257                    ; оттенки бревна и торца (init_blk_lut)
lut_logend: resd 257
; свет и тени
alignb 64
slightL:    resd SLW*SLW                ; уличные фонари
slightB:    resd SLW*SLW                ; сломанный фонарь
slightT:    resd SLW*SLW                ; телевизоры
aomap:      resb SLW*SLW
alignb 16
mzg:        resd GW*GW                  ; сэмпл: горизонт (для частиц), облака, 0, 0
lgv:        resd GW*GW                  ; видимое игроком: фонарик, аура, обзор (до сглаживания)
lgt:        resd GW*GW
vgrid:      resb GW*GW                  ; видимость без сглаживания — кого рисовать
; тени луны, 8 ячеек на тайл
mz8:        resd SW8*SW8                ; ячейка развёртки: горизонт+64 (слово), 4096/полутень
alignb 16
cols:       resb 4*SWD                  ; дорожки развёртки (две): высоты луча, пути от заслонителя
msh:        resq NN                     ; сдвиг выборки к луне по нормали, 8.8
wt8x:       resq 32                     ; веса 2x2: (32−f, f, 32−f, f)
wt8y:       resq 32                     ;           (32−f, 32−f, f, f)
stamps:     resq MAXSTAMP               ; вписанные ячейки фигур: смещение, старый байт
prof:       resb 6*64                   ; тела вращения спрайтов: 32 высоты, 32 плотности
hmap8:      resb HW8*HW8                ; биты 0-6 — высота заслонителя, px; бит 7 — под крышей
nbuf:       resb SCR_W*SCR_H            ; нормаль пикселя (NOWN — свой свет)
alignb 16
lbuf:       resd SCR_W*SCR_H            ; свой свет пикселя (крыши): B,G,R,видимость
moon_n:     resq NN                     ; луна·N·L по нормали (слова)
amb_d:      resq NN                     ; недобор неба по нормали (слова)
amb_n:      resd NN                     ; небо по нормали (байты) — для крыш
nvec:       resd NN*4
clouds:     resb 4096
cl_tab:     resb 256
cl_f:       resd 4096
cl_g:       resd 4096
occ:        resb MAXOCC*OC_SZ
hflags:     resb MAPW*MAPW
; частицы и постобработка
alignb 16
parts:      resb MAXP*P_SZ
blA:        resq BLW*BLH
blB:        resq BLW*BLH
lamps:      resd MAXLAMP
wires:      resq MAXWIRE
smk:        resw MAXSMK
smkz:       resb MAXSMK                 ; высота жерла трубы, px
alignb 16
houses:     resb MAXH*HS_SZ
rmap:       resb MAPW*MAPW              ; номер дома + 1 под крышей
alignb 16
drift:      resd DECW*DECW              ; рельеф снега: высота 1/8 px, наклон нормали x, y
; спрайты мелочей
alignb 16
spr_snowman: resd 14*22
spr_bench:  resd 20*12
spr_line:   resd 32*26
spr_linei:  resd 32*26
spr_kennel: resd 22*18
spr_birdh:  resd 8*46
spr_well:   resd 40*58
spr_chim:   resd 8*24
spr_chims:  resd 8*24
; ветер
alignb 16
wob:        resb WSW*WSW                ; заслоны ветру, px (2 ячейки на тайл)
wsh:        resb WSW*WSW                ; затишье 0..255
ws_a:       resd WSW + 4                ; дорожки развёртки затишья (подряд!)
ws_b:       resd WSW + 4
wsl_lee:    resb 128                    ; высота заслона -> доля затишья: за ним
wsl_front:  resb 128                    ;                                  перед ним
gustn:      resb 64*64                  ; шум порывов
strk:       resb 256*64                 ; полосы позёмки
strk_acc:   resw 256*64
wgrid:      resb GW*GW + 16             ; маска позёмки на кадр (как сетка света)
frost:      resb SCR_W*SCR_H            ; иней по краям кадра
