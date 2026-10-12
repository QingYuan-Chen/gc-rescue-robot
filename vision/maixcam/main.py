
from maix import camera, display, image, nn, app, uart, err, pinmap, time, touchscreen
import struct, os

# 网络字节序（大端）：4 个 uint8_t 和 4 个 uint16_t，共 12 字节载荷。
PACK_FMT = "!BBBBHHHH"

class VisionManager:
    def __init__(self, team=0, bolls_num=0, is_valid=0, go_on=0, x=0, y=0, w=0, h=0):
        self.team = team
        self.bolls_num = bolls_num
        self.is_valid = is_valid
        self.go_on = go_on
        self.x = x
        self.y = y
        self.w = w
        self.h = h
        self.last_base_rects = []   # 保存最近一次检测到的所有基地矩形，每个为 (x, y, w, h)
        self.base_miss_counter = 0

    def pack(self) -> bytes:
        """将当前状态打包为不含帧头的 12 字节载荷。"""
        return struct.pack(
            PACK_FMT,
            self.team,
            self.bolls_num,
            self.is_valid,
            self.go_on,
            self.x,
            self.y,
            self.w,
            self.h
        )

    @staticmethod
    def unpack(data: bytes) -> "VisionManager":
        """解析不含帧头的 12 字节载荷，返回 VisionManager 对象。"""
        if len(data) != 12:
            raise ValueError("数据包必须是12字节")
        t, bn, iv, go, x, y, w, h = struct.unpack(PACK_FMT, data)
        return VisionManager(team=t, bolls_num=bn, is_valid=iv, go_on=go, x=x, y=y, w=w, h=h)

    def send(self, serial):
        """通过 UART 发送 0x55 0xAA 帧头和 12 字节载荷。"""
        raw = self.pack()
        pkt = b'\x55\xAA' + raw
        serial.write(pkt)

    def __repr__(self):
        """返回用于调试的对象字段描述。"""
        return (
            f"VisionManager(team={self.team}, bolls_num={self.bolls_num}, "
            f"is_valid={self.is_valid}, go_on={self.go_on}, x={self.x}, y={self.y}, w={self.w}, h={self.h})"
        )

vision = VisionManager()

debugger = 0x00
bolls_mode = 0x01
confirm_mode = 0x02
cross_mode = 0x03
base_mode = 0x04

blue_team = 0x01
red_team = 0x02

first_catch_flag = True

max_bolls = 3

pin_function = {
        "A16": "UART0_TX",
        "A17": "UART0_RX"
    }
device = "/dev/ttyS0"

for pin, func in pin_function.items():
    err.check_raise(pinmap.set_pin_function(pin, func), f"Failed set pin{pin} function to {func}")

# 初始化串口。
serial = uart.UART(device, 115200)


ts = touchscreen.TouchScreen()
exit_x = 0
exit_y = 0
exit_w = 64
exit_h = 48
exit_word_x = 0
exit_word_y = 10

blue_x = 80
blue_y = 0
blue_w = 64
blue_h = 48
blue_word_x = 80
blue_word_y = 10

red_x = 160
red_y = 0
red_w = 64
red_h = 48
red_word_x = 160
red_word_y = 10
img_rect_color = image.COLOR_WHITE

img_x1 = 0
img_y1 = 50
img_w1 = 224
img_h1 = 174


model_path = "/root/models/gc/290808/model_290808.mud"
detector = nn.YOLOv5(model=model_path)

# 标签顺序与当前模型的 class_id 一一对应。
labels = ["black", "blue_base", "red_base", "cross", "yellow", "red", "blue"]

# 各模式允许识别的标签集合
mode_filter = {
    debugger:       {"black", "yellow", "red", "blue", "cross", "blue_base", "red_base"},
    bolls_mode:     {"black", "yellow", "red", "blue", "blue_base", "red_base"},
    confirm_mode:   {"black", "yellow", "red", "blue"},
    cross_mode:     {"cross"},
    base_mode:      {"blue_base", "red_base"}
}

# 模式名称映射，用于左下角打印
mode_name_map = {
    debugger: "Debug",
    bolls_mode: "Balls",
    confirm_mode: "Confirm",
    cross_mode: "Cross",
    base_mode: "Base"
}

cam = camera.Camera(detector.input_width(), detector.input_height(), detector.input_format())
dis = display.Display()
current_mode = debugger # 上电默认调试模式

while not app.need_exit():
    while True:
        b1 = serial.read(1)
        if not b1:
            break
        # 匹配帧头的第一个字节 0x55。
        if b1[0] == 0x55:
            b2 = serial.read(1)
            if len(b2) and b2[0] == 0xAA:
                # 匹配完整的双字节帧头后读取模式号。
                cmd_buf = serial.read(1)
                if len(cmd_buf) == 1:
                    cmd = cmd_buf[0]
                    if cmd in mode_filter and cmd != current_mode:
                        print(f"收到有效模式指令: {cmd}")
                        current_mode = cmd

    img = cam.read()

    img.draw_rect(exit_x,exit_y,exit_w,exit_h,img_rect_color,1)
    img.draw_string(exit_word_x,exit_word_y,"exit",image.COLOR_WHITE,scale = 2)
    img.draw_rect(blue_x,blue_y,blue_w,blue_h,img_rect_color,1)
    img.draw_string(blue_word_x,blue_word_y,"blue",image.COLOR_WHITE,scale = 2)
    img.draw_rect(red_x,red_y,red_w,red_h,img_rect_color,1)
    img.draw_string(red_word_x,red_word_y,"red",image.COLOR_WHITE,scale = 2)
    x, y, pressed = ts.read()
    img_w = img.width()
    img_h = img.height()
    scr_w = dis.width()
    scr_h = dis.height()
    map_x = x * img_w / scr_w
    map_y = y * img_h / scr_h
    img.draw_rect(img_x1, img_y1, img_w1, img_h1, color = image.COLOR_RED)

    if pressed and (exit_x <= map_x <= exit_x + exit_w and exit_y <= map_y <= exit_y + exit_h):
        app.set_exit_flag(True)

    if pressed and (blue_x <= map_x <= blue_x + blue_w and blue_y <= map_y <= blue_y + blue_h):
        vision.team = blue_team
        img_rect_color = image.COLOR_BLUE

    if pressed and (red_x <= map_x <= red_x + red_w and red_y <= map_y <= red_y + red_h):
        vision.team = red_team
        img_rect_color = image.COLOR_RED
    
    objs_filtered = []

    # 当前模式在过滤表中时执行检测；上电默认调试模式也满足此条件。
    if current_mode in mode_filter:
        allow_labels = mode_filter[current_mode]

        # 全图检测
        all_objs = detector.detect(img, conf_th = 0.35, iou_th = 0.45)
        for obj in all_objs:
            cls_name = labels[obj.class_id]
            if cls_name in allow_labels:
                objs_filtered.append(obj)
        
        # 绘制过滤后的目标框与文字。
        for obj in objs_filtered:
            cls_name = labels[obj.class_id]
            score = obj.score
            img.draw_rect(obj.x, obj.y, obj.w, obj.h, color = image.COLOR_RED)
            cx = int(obj.x + obj.w / 2)
            cy = int(obj.y + obj.h / 2)
            msg = f'{cls_name}: {score:.2f}'
            img.draw_string(obj.x, obj.y, msg, color = image.COLOR_RED)
            coord_msg = f"({cx},{cy})"
            img.draw_string(obj.x, obj.y + 20, coord_msg, color=image.COLOR_GREEN, scale=1)
            coord_msg = f"({obj.w},{obj.h})"
            img.draw_string(obj.x, obj.y + 40, coord_msg, color=image.COLOR_GREEN, scale=1)
    
        self_label = "blue" if vision.team == blue_team else "red"
        enemy_label = "red" if vision.team == blue_team else "blue"
        if(current_mode == bolls_mode):
            base_objs = [obj for obj in objs_filtered if labels[obj.class_id] in ["blue_base", "red_base"]]
            if base_objs:
                # 记录当前帧所有基地的矩形
                vision.last_base_rects = [(int(b.x), int(b.y), int(b.w), int(b.h)) for b in base_objs]
                vision.base_miss_counter = 0      # 检测到了，计数器清零
            else:
                vision.base_miss_counter += 1
                if vision.base_miss_counter > 5:  # 连续超过 5 帧未检测到基地时清空记忆
                    vision.last_base_rects = []
            # 使用最近一次检测到的基地矩形；连续丢失超过 5 帧后不再沿用。
            active_rects = vision.last_base_rects
            all_out_balls = []
            for obj in objs_filtered:
                cls = labels[obj.class_id]
                if vision.go_on and cls == "yellow":
                    continue
                # 计算球中心点
                ball_cx = obj.x + obj.w / 2
                ball_cy = obj.y + obj.h / 2
                in_base_area = False
                
                # 将水平范围内、中心位于基地顶边或下方的目标判入基地区域。
                for bx, by, bw, bh in active_rects:
                    if bx <= ball_cx <= bx + bw and by <= ball_cy:
                        in_base_area = True
                        break
                # 仅将未落入上述基地区域的目标加入候选列表。
                if not in_base_area:
                    all_out_balls.append(obj)

            # 候选类别仅保留己方球、黑球和黄球。
            valid_balls = []
            for obj in all_out_balls:
                cls = labels[obj.class_id]
                if cls in (self_label, "black", "yellow"):
                    valid_balls.append(obj)

            def get_ball_priority(obj):
                name = labels[obj.class_id]
                if name == "yellow":
                    return 0
                elif name == "black":
                    return 1
                elif (vision.team == blue_team and name == "blue") or (vision.team == red_team and name == "red"):
                    return 2
                else:
                    return 99  # 其他类别使用最低优先级
            candidate_balls = []
            # 首次抓取仅保留己方球，随后按优先级排序。
            if first_catch_flag:
                vision.go_on = 0
                for obj in valid_balls:
                    if labels[obj.class_id] == self_label:
                        candidate_balls.append(obj)
            else:
                candidate_balls = valid_balls
            ball_objs = sorted(candidate_balls, key=lambda o: get_ball_priority(o))

            vision.bolls_num = len(ball_objs)
            vision.is_valid = 0
            vision.go_on = 0

            if vision.bolls_num > 0:
                best_ball = ball_objs[0]
                vision.x = int(best_ball.x + best_ball.w/2)
                vision.y = int(best_ball.y + best_ball.h/2)
                vision.w = int(best_ball.w)
                vision.h = int(best_ball.h)
                vision.is_valid = 1
        
        elif(current_mode == confirm_mode):

            # 按目标中心筛选落在夹爪区域内的球。
            clip_balls = []
            for obj in objs_filtered:
                cls = labels[obj.class_id]
                if cls not in ["black", "yellow", "red", "blue"]:
                    continue
                # 物体中心坐标
                if first_catch_flag and cls != self_label:
                    continue
                cx = obj.x + obj.w / 2
                cy = obj.y + obj.h / 2
                if img_x1 <= cx <= img_x1 + img_w1 and img_y1 <= cy <= img_y1 + img_h1:
                    clip_balls.append(cls)

            total_clip_ball = len(clip_balls)
            has_enemy = enemy_label in clip_balls
            yellow_cnt = clip_balls.count("yellow")
            self_cnt = clip_balls.count(self_label)
            black_cnt = clip_balls.count("black")
            vision.bolls_num = total_clip_ball

            # 根据球的类别、数量和首次抓取状态更新有效性与继续收集标志。
            vision.bolls_num = total_clip_ball
            if total_clip_ball > 0:
                if has_enemy:
                    vision.is_valid = 0
                    vision.go_on = 0
                elif yellow_cnt > 0:
                    # 有黄色球
                    if yellow_cnt == 1 and total_clip_ball == 1:
                        vision.go_on = 0
                        vision.is_valid = 1
                    else:
                        vision.go_on = 0
                        vision.is_valid = 0
                else:
                    # 区域内没有敌方球和黄球，仅包含己方球与黑球。
                    if self_cnt + black_cnt < max_bolls:
                        if first_catch_flag:
                            if self_cnt == 1 and total_clip_ball == 1:
                                vision.go_on = 0
                                vision.is_valid = 1
                            else:
                                vision.go_on = 0
                                vision.is_valid = 0
                        else:
                            vision.go_on = 1
                            vision.is_valid = 1
                    else:
                        vision.go_on = 0
                        vision.is_valid = 1
            else:
                vision.is_valid = 0
                vision.go_on = 0
        
        elif(current_mode == cross_mode):
            first_catch_flag = False
            cross_list = [o for o in objs_filtered if labels[o.class_id] == "cross"]
            vision.bolls_num = len(cross_list)
            if vision.bolls_num > 0:
                target = cross_list[0]
                vision.x = int(target.x + target.w/2)
                vision.y = int(target.y + target.h/2)
                vision.w = int(target.w)
                vision.h = int(target.h)

        
        elif(current_mode == base_mode):
            target_base_name = "blue_base" if vision.team == blue_team else "red_base"
            base_list = [o for o in objs_filtered if labels[o.class_id] == target_base_name]
            vision.bolls_num = len(base_list)
            if vision.bolls_num > 0:
                target = base_list[0]
                vision.x = int(target.x + target.w/2)
                vision.y = int(target.y + target.h/2)
                vision.w = int(target.w)
                vision.h = int(target.h)
                vision.go_on = 0

        # 保留原流程中的载荷打包，实际发送时 send() 会再次打包。
        buf = vision.pack()

        # 发送帧头和当前状态载荷，此调用不会清空接收缓冲区。
        vision.send(serial)
        
    # 左下角打印当前工作模式
    mode_text = mode_name_map.get(current_mode, "Unknown")
    # 坐标：左下角 x=10，y=img.height()-30，白色字体
    img.draw_string(10, img.height() - 30, f"Mode:{mode_text}", image.COLOR_WHITE, scale=2)

    dis.show(img)
    time.sleep_ms(5)

