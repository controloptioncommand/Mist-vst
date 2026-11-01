# Нарезает скриншоты рендер-теста для мануала: python3 crop.py <папка со скриншотами> <папка img>
import sys
from PIL import Image

src, out = sys.argv[1].rstrip("/") + "/", sys.argv[2].rstrip("/") + "/"


def save(im, name):
    im.convert("RGB").save(out + name + ".jpg", quality=88)


# координаты — в базовом размере окна 1120×860
main = Image.open(src + "page0_rubber.png")
save(main, "main")
save(main.crop((14, 348, 314, 846)), "slots")                    # колонка операторов
save(main.crop((326, 786, 1106, 846)), "footer")                 # подвал
for i in range(7):                                               # панель с вкладками
    save(Image.open(src + f"page{i}_rubber.png").crop((326, 350, 1106, 846)), f"tab{i}")
for v in ("scope", "waves", "spectrum"):                         # главный вид
    save(Image.open(src + f"ui_rubber_{v}.png").crop((14, 76, 1106, 338)), "viz_" + v)
save(Image.open(src + "ui_oxygen_scope.png"), "ui_oxygen_scope")
