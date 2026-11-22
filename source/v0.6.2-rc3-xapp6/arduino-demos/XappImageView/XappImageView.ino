#include <Arduino.h>  // Arduino core types and timing functions. / Arduino 核心型別與計時函式。
#include <lvgl.h>  // LVGL widgets and rendering API supplied by XDOS. / XDOS 提供的 LVGL 元件和繪圖介面。
#include <fatfs/ff.h>  // FatFs file and directory API. / FatFs 檔案與目錄介面。
#include <strings.h>
#include <string.h>
#include <stdio.h>

namespace {
lv_obj_t *image, *status, *picker;
lv_image_dsc_t picture;
uint16_t *pixels;

uint32_t le(const uint8_t *p, unsigned size)
{
    uint32_t value=0;
    for (unsigned i=0;i<size;++i) value|=uint32_t(p[i])<<(8*i);
    return value;
}

bool read(FIL &file, void *data, UINT size)
{
    UINT got=0;
    return f_read(&file,data,size,&got)==FR_OK && got==size;
}

void clearImage()
{
    if (!pixels) return;
    lv_image_set_src(image,nullptr);
    lv_free(pixels);
    pixels=nullptr;
}

bool loadBmp(const char *path)
{
    FIL file;
    if (f_open(&file,path,FA_READ)!=FR_OK) return false;  // Open the selected file read-only. / 以唯讀模式開啟所選檔案。
    uint8_t header[54];
    bool ok=read(file,header,sizeof header);
    if (!ok || header[0]!='B' || header[1]!='M' || le(header+14,4)<40 ||
        le(header+26,2)!=1 || le(header+30,4)!=0) {
        f_close(&file);  // Release the FAT volume file handle. / 關閉 FAT 磁碟區上的檔案。
        return false;
    }
    const uint32_t offset=le(header+10,4);  // Read the byte offset of the BMP pixel data. / 讀取 BMP 像素資料的位元組偏移。
    const uint32_t width=le(header+18,4);
    const int32_t signedHeight=int32_t(le(header+22,4));
    const uint32_t height=signedHeight<0 ? 0u-uint32_t(signedHeight) : uint32_t(signedHeight);
    const unsigned bytes=le(header+28,2)/8;
    if (!width || !height || width>2048 || height>2048 ||
        (le(header+28,2)!=24 && le(header+28,2)!=32) ||
        offset<54 || width*height>2304000) {
        f_close(&file);  // Release the FAT volume file handle. / 關閉 FAT 磁碟區上的檔案。
        return false;
    }
    const unsigned rowBytes=(width*bytes+3)&~3u;  // Account for BMP rows padded to four-byte boundaries. / 計入 BMP 每列的四位元組對齊填補。
    if (offset>f_size(&file) || uint64_t(rowBytes)*height>f_size(&file)-offset) {
        f_close(&file);  // Release the FAT volume file handle. / 關閉 FAT 磁碟區上的檔案。
        return false;
    }
    auto *row=static_cast<uint8_t *>(lv_malloc(rowBytes));  // Allocate one source row for decoding. / 配置一列來源像素的解碼緩衝區。
    auto *data=static_cast<uint16_t *>(lv_malloc(width*height*2));  // Allocate the decoded RGB565 image buffer. / 配置 RGB565 解碼結果的緩衝區。
    ok=row && data && f_lseek(&file,offset)==FR_OK;
    for (unsigned y=0;y<height && ok;++y) {
        ok=read(file,row,rowBytes);
        if (!ok) break;
        auto *to=data+(signedHeight<0 ? y : height-1-y)*width;  // Handle both top-down and bottom-up BMPs. / 處理由上往下及由下往上儲存的 BMP。
        for (unsigned x=0;x<width;++x) {
            const auto *bgr=row+x*bytes;
            to[x]=uint16_t((bgr[2]>>3)<<11 | (bgr[1]>>2)<<5 | (bgr[0]>>3));
        }
    }
    f_close(&file);  // Release the FAT volume file handle. / 關閉 FAT 磁碟區上的檔案。
    lv_free(row);  // Free the temporary decoding row. / 釋放單列解碼用的暫存緩衝區。
    if (!ok) { lv_free(data); return false; }

    clearImage();  // Detach and free the previous image buffer. / 解除前一張圖片並釋放其緩衝區。
    pixels=data;
    picture={};
    picture.header.magic=LV_IMAGE_HEADER_MAGIC;
    picture.header.cf=LV_COLOR_FORMAT_RGB565;
    picture.header.w=width;
    picture.header.h=height;
    picture.header.stride=width*2;
    picture.data_size=width*height*2;
    picture.data=reinterpret_cast<const uint8_t *>(pixels);
    lv_image_set_src(image,&picture);  // Display the newly decoded image. / 顯示剛解碼的圖片。
    auto *screen=lv_screen_active();
    const unsigned scaleX=unsigned(lv_obj_get_width(screen)-32)*256/width;
    const unsigned scaleY=unsigned(lv_obj_get_height(screen)-150)*256/height;
    lv_image_set_scale(image,LV_MIN(256u,LV_MIN(scaleX,scaleY)));  // Fit the image on screen without enlarging it. / 縮小圖片以符合畫面，保留小圖原始大小。
    lv_obj_align(image,LV_ALIGN_CENTER,0,39);
    return true;
}

void changed(lv_event_t *)
{
    if (!lv_dropdown_get_selected(picker)) return;
    char path[512];
    lv_dropdown_get_selected_str(picker,path,sizeof path);  // Copy the selected file path from the list. / 從清單複製選取的檔案路徑。
    lv_label_set_text(status,loadBmp(path) ? path : "Cannot display this BMP.");
}
}

void setup()  // Run once when the sketch starts. / 草稿啟動時執行一次。
{
    auto *screen=lv_screen_active();
    image=lv_image_create(screen);
    status=lv_label_create(screen);
    lv_obj_set_width(status,lv_pct(92));
    lv_label_set_long_mode(status,LV_LABEL_LONG_DOT);
    lv_label_set_text(status,"24-bit or 32-bit uncompressed BMP");
    lv_obj_align(status,LV_ALIGN_TOP_MID,0,86);
    picker=lv_dropdown_create(screen);
    lv_obj_set_width(picker,lv_pct(92));
    lv_obj_align(picker,LV_ALIGN_TOP_MID,0,48);
    lv_dropdown_set_options(picker,"Select a BMP file");
    const char *folders[]={"SD:/","SD:/XDOS/IMAGES/","SD:/XDOS/INBOX/","USB:/","USB:/IMAGES/"};
    unsigned listed=0;
    for (const char *folder : folders) {
        DIR dir;
        if (f_opendir(&dir,folder)!=FR_OK) continue;
        FILINFO file;
        while (listed<128 && f_readdir(&dir,&file)==FR_OK && file.fname[0]) {
            const auto length=strlen(file.fname);
            if ((file.fattrib&AM_DIR) || length<=4 || strcasecmp(file.fname+length-4,".bmp") ||
                strchr(file.fname,'\n')) continue;
            char path[512];
            const int size=snprintf(path,sizeof path,"%s%s",folder,file.fname);
            if (size<0 || unsigned(size)>=sizeof path) continue;
            lv_dropdown_add_option(picker,path,LV_DROPDOWN_POS_LAST);  // Append this BMP to the picker. / 將這張 BMP 加入選檔清單。
            ++listed;
        }
        f_closedir(&dir);  // Release the directory handle. / 關閉目錄。
    }
    if (!listed) lv_label_set_text(status,"No BMP files found on SD or USB.");
    lv_obj_add_event_cb(picker,changed,LV_EVENT_VALUE_CHANGED,nullptr);  // Load a BMP when the selection changes. / 選取項目變更時載入對應 BMP。
}

void loop() {}

extern "C" void xdos_stop_image_view()
{
    clearImage();  // Detach and free the previous image buffer. / 解除前一張圖片並釋放其緩衝區。
}

extern "C" void xdos_sketch_stop() { xdos_stop_image_view(); }  // Expose image-buffer cleanup to the launcher. / 讓啟動器可清理圖片緩衝區。
