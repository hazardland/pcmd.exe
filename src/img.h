// MODULE: img
// Purpose : SIXEL-first still-image command
// Exports : img_cmd()
// Depends : common.h, sixel.h, stb_image.h

static std::vector<uint8_t> img_resize_rgb(const uint8_t* src, int src_w, int src_h, int dst_w, int dst_h) {
    std::vector<uint8_t> dst((size_t)dst_w * dst_h * 3);
    for (int y = 0; y < dst_h; ++y) {
        int sy = ((y * 2 + 1) * src_h) / (dst_h * 2);
        if (sy >= src_h) sy = src_h - 1;
        for (int x = 0; x < dst_w; ++x) {
            int sx = ((x * 2 + 1) * src_w) / (dst_w * 2);
            if (sx >= src_w) sx = src_w - 1;
            const uint8_t* s = src + ((size_t)sy * src_w + sx) * 3;
            uint8_t* d = dst.data() + ((size_t)y * dst_w + x) * 3;
            d[0] = s[0];
            d[1] = s[1];
            d[2] = s[2];
        }
    }
    return dst;
}

int img_cmd(const std::string& line) {
    std::string path = line.size() > 3 ? line.substr(3) : "";
    while (!path.empty() && path.front() == ' ') path.erase(path.begin());
    while (!path.empty() && path.back()  == ' ') path.pop_back();

    if (path.empty()) {
        out("img: usage: img <path>\r\n");
        return 1;
    }
    if (!sixel_supported()) {
        out("img: SIXEL is not supported in this terminal\r\n");
        return 1;
    }

    std::string norm = normalize_path(path);
    int src_w = 0, src_h = 0;
    uint8_t* src = stbi_load(norm.c_str(), &src_w, &src_h, nullptr, 3);
    if (!src) {
        out("img: cannot load image '" + path + "'\r\n");
        return 1;
    }

    SixelFit fit = sixel_fit(src_w, src_h);
    std::vector<uint8_t> resized = img_resize_rgb(src, src_w, src_h, fit.pixel_w, fit.pixel_h);
    stbi_image_free(src);

    SixelFrame frame = { resized.data(), fit.pixel_w, fit.pixel_h };
    if (sixel_render(frame, fit) != 0) {
        out("img: failed to render SIXEL output\r\n");
        return 1;
    }

    out("\r\n");
    return 0;
}
