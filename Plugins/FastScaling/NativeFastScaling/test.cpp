#include "catch.hpp"

#include "fastscaling_private.h"
#include "weighting_test_helpers.h"
#include "trim_whitespace.h"
#include "string.h"

std::ostream& operator<<(std::ostream& out, const BitmapFloat & bitmap_float)
{
    return out << "BitmapFloat: w:" << bitmap_float.w << " h: " << bitmap_float.h << " channels:" << bitmap_float.channels << '\n';
}

TEST_CASE("Argument checking for convert_sgrp_to_linear", "[error_handling]") {
    Context context;
    Context_initialize(&context);
    BitmapBgra * src = create_bitmap_bgra(&context, 2, 3, true, Bgra32);
    char error_msg[1024];
    CAPTURE(Context_last_error_message(&context, error_msg, sizeof error_msg));
    REQUIRE(src != NULL);
    BitmapFloat * dest = create_bitmap_float(1, 1, 4, false);
    convert_srgb_to_linear(src, 3, dest, 0, 0);
    destroy_bitmap_bgra(src);
    CAPTURE(*dest);
    REQUIRE(dest->float_count == 4); // 1x1x4 channels
    destroy_bitmap_float(dest);
}

TEST_CASE("With context", "[error_handling]") {
    Context context;
    Context_initialize(&context);
    BitmapBgra * source = create_bitmap_bgra(&context, 1, 1, true, (BitmapPixelFormat)2);
    destroy_bitmap_bgra(source);
}

bool test (int sx, int sy, BitmapPixelFormat sbpp, int cx, int cy, BitmapPixelFormat cbpp, bool transpose, bool flipx, bool flipy, bool profile, InterpolationFilter filter)
{
    Context context;
    Context_initialize(&context);
    BitmapBgra * source = create_bitmap_bgra(&context, sx, sy, true, sbpp);
    BitmapBgra * canvas = create_bitmap_bgra(&context, cx, cy, true, cbpp);

    RenderDetails * details = create_render_details();

    details->interpolation = create_interpolation(filter);

    details->sharpen_percent_goal = 50;
    details->post_flip_x = flipx;
    details->post_flip_y = flipy;
    details->post_transpose = transpose;
    details->enable_profiling = profile;


    Renderer * p = create_renderer(source, canvas, details);

    perform_render(&context, p);

    destroy_renderer(p);

    destroy_bitmap_bgra(source);
    destroy_bitmap_bgra(canvas);

    free_lookup_tables();
    return true;
}


bool test_in_place (int sx, int sy, BitmapPixelFormat sbpp, bool flipx, bool flipy, bool profile, float sharpen, uint32_t kernelRadius)
{
    Context context;
    BitmapBgra * source = create_bitmap_bgra(&context, sx, sy, true, sbpp);

    RenderDetails * details = create_render_details ();


    details->sharpen_percent_goal = sharpen;
    details->post_flip_x = flipx;
    details->post_flip_y = flipy;
    details->enable_profiling = profile;
    if (kernelRadius > 0) {
        details->kernel_a = create_guassian_kernel_normalized (1.4, kernelRadius);
    }


    Renderer * p = create_renderer_in_place (source, details);
    
    perform_render(&context, p);

    destroy_renderer(p);

    destroy_bitmap_bgra(source);

    free_lookup_tables();
    return true;
}


TEST_CASE( "Render without crashing", "[fastscaling]") {
    REQUIRE (test (400, 300, Bgra32, 200, 40, Bgra32, false, false, false, false, (InterpolationFilter)0));
}

TEST_CASE( "Render - upscale", "[fastscaling]") {
    REQUIRE (test (200, 40, Bgra32, 500, 300, Bgra32, false, false, false, false, (InterpolationFilter)0));
}

TEST_CASE("Render - downscale 24->32", "[fastscaling]") {
    REQUIRE (test (400, 200, Bgr24, 200, 100, Bgra32, false, false, false, false, (InterpolationFilter)0));
}

TEST_CASE("Render and rotate", "[fastscaling]") {
    REQUIRE (test (200, 40, Bgra32, 500, 300, Bgra32, true, true, true, false, (InterpolationFilter)0));
}

TEST_CASE("Render and rotate with profiling", "[fastscaling]") {
    REQUIRE (test (200, 40, Bgra32, 500, 300, Bgra32, true, true, true, true, (InterpolationFilter)0));
}

TEST_CASE ("Flip in place", "[fastscaling]") {
    REQUIRE (test_in_place (400, 300, Bgra32, true, true, false, 0, 0));
}
TEST_CASE ("Flip in place 24 bit", "[fastscaling]") {
    REQUIRE (test_in_place (400, 300, Bgr24, true, true, false, 0, 0));
}
//segfaults the process
TEST_CASE ("Sharpen and convolve in place", "[fastscaling]") {
    REQUIRE (test_in_place (400, 300, Bgr24, false, false, false, 0.5, 0));
}
//*/

BitmapBgra*  crop_window (BitmapBgra* source, uint32_t x, uint32_t y, uint32_t w, uint32_t h){
    Context context;
    BitmapBgra* cropped = create_bitmap_bgra_header(&context, w, h);
    cropped->fmt = source->fmt;
    const uint32_t bytes_pp = BitmapPixelFormat_bytes_per_pixel(source->fmt);
    cropped->pixels = source->pixels + (y * source->stride) + (x * bytes_pp);
    cropped->stride = source->stride;
    return cropped;
}

void clear_bitmap (BitmapBgra* b, uint8_t fill_red, uint8_t fill_green, uint8_t fill_blue, uint8_t fill_alpha){
    const uint32_t bytes_pp = BitmapPixelFormat_bytes_per_pixel (b->fmt);
    const uint32_t row_bytes = bytes_pp * b->w;
    for (uint32_t i = 0; i < row_bytes; i+=4){
        b->pixels[i] = fill_blue;
        b->pixels[i+ 1] = fill_green;
        b->pixels[i + 2] = fill_red;
        if (bytes_pp == 4){
            b->pixels[i + 3] = fill_alpha;
        }
    }
    for (uint32_t i = 0; i < b->h; i++){
        memcpy (b->pixels + (i * b->stride), b->pixels, row_bytes);
    }
}

void fill_rect (BitmapBgra* b, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint8_t fill_red, uint8_t fill_green, uint8_t fill_blue, uint8_t fill_alpha){
    BitmapBgra * cropped = crop_window (b, x, y, w, h);
    clear_bitmap (cropped, fill_red, fill_green, fill_blue, fill_alpha);
    destroy_bitmap_bgra (cropped);
}

/*/ segfaults the process if you uncomment this
TEST_CASE ("Trim whitespace in 32-bit image", "[fastscaling]") {
    BitmapBgra* b = create_bitmap_bgra (200, 150, true, Bgra32);

    fill_rect (b, 30, 20, 100, 75, 30, 30, 30, 255);

    Rect r = detect_content (b, 20);

    CHECK (r.x1 == 30);
    CHECK (r.y1 == 20);
    CHECK (r.x2 == 20 + 75);
    CHECK (r.y2 == 30 + 100);

    destroy_bitmap_bgra (b);
}

//*/


TEST_CASE("Test contrib windows", "[fastscaling]") {

    char msg[256];
    bool r = test_contrib_windows(msg);

    if (!r) FAIL(msg);
    REQUIRE(r);
}

TEST_CASE("Test Weighting", "[fastscaling]") {

    char msg[256];


    //These have window = 1, and shouldnt' have negative values. They should also end at 1
    CHECK(test_filter(InterpolationFilter::Filter_Hermite, msg, 0, 0, 0.99, 0.08, 1) == nullptr);
    //Also called a linear filter
    CHECK(test_filter(InterpolationFilter::Filter_Triangle, msg, 0, 0, 0.99, 0.08, 1) == nullptr);
    //Box should only return a value from -0.5..0.5
    CHECK(test_filter(InterpolationFilter::Filter_Box, msg, 0, 0, 0.51, 0.001, 0.51) == nullptr);



    //These should go negative between x=1 and x=2, but should end at x=2
    CHECK(test_filter(InterpolationFilter::Filter_CatmullRom, msg, 1, 2, 1, 0.08, 2) == nullptr);
    CHECK(test_filter(InterpolationFilter::Filter_CubicFast, msg, 1, 2, 1, 0.08, 2) == nullptr);
    CHECK(test_filter(InterpolationFilter::Filter_Cubic, msg, 1, 2, 1, 0.08, 2) == nullptr);

    //BSpline is a smoothing filter, always positive
    CHECK(test_filter(InterpolationFilter::Filter_CubicBSpline, msg, 0, 0, 1.75, 0.08, 2) == nullptr);

    CHECK(test_filter(InterpolationFilter::Filter_Mitchell, msg, 1.0f, 1.75f, 1, 0.08, 1.75) == nullptr);


    CHECK(test_filter(InterpolationFilter::Filter_Robidoux, msg, 1, 1.65, 1, 0.08, 1.75) == nullptr);
    CHECK(test_filter(InterpolationFilter::Filter_RobidouxSharp, msg, 1, 1.8, 1, 0.08, 1.8) == nullptr);


    //Sinc filters. These have second crossings.
    CHECK(test_filter(InterpolationFilter::Filter_Lanczos2, msg, 1, 2, 1, 0.08, 2) == nullptr);
    CHECK(test_filter(InterpolationFilter::Filter_Lanczos2Sharp, msg, 0.954, 1.86, 1, 0.08, 2) == nullptr);

    //These should be negative between x=1 and x=2, positive between 2 and 3, but should end at 3

    CHECK(test_filter(InterpolationFilter::Filter_Lanczos3, msg, 1, 2, 1, 0.1, 3) == nullptr);
    CHECK(test_filter(InterpolationFilter::Filter_Lanczos3Sharp, msg, 0.98, 1.9625, 1, 0.1, 2.943) == nullptr);

    ///
    CHECK(test_filter(InterpolationFilter::Filter_Lanczos2Windowed, msg, 1, 2, 1, 0.08, 2) == nullptr);

    CHECK(test_filter(InterpolationFilter::Filter_Lanczos2SharpWindowed, msg, 0.954, 1.86, 1, 0.08, 2) == nullptr);

    //These should be negative between x=1 and x=2, positive between 2 and 3, but should end at 3

    CHECK(test_filter(InterpolationFilter::Filter_Lanczos3Windowed, msg, 1, 2, 1, 0.1, 3) == nullptr);


    CHECK(test_filter(InterpolationFilter::Filter_Lanczos3SharpWindowed, msg, 0.98, 1.9625, 1, 0.1, 2.943) == nullptr);

}




TEST_CASE("Test Linear RGB 000 -> LUV ", "[fastscaling]") {
    float bgra[4] = { 0, 0, 0, 0 };

    linear_to_luv(bgra);

    CHECK(bgra[0] == 0.0f);
    CHECK(bgra[1] == 100.0f);
    CHECK(bgra[2] == 100.0f);
}

TEST_CASE("Roundtrip RGB<->LUV 0.2,0.2,0.2 ", "[fastscaling]") {
    float bgra[4] = { 0.2f, 0.2f, 0.2f, 1 };

    linear_to_luv(bgra);
    luv_to_linear(bgra);

    CHECK(bgra[0] == Approx(0.2f));
    CHECK(bgra[1] == Approx(0.2f));
    CHECK(bgra[2] == Approx(0.2f));
}


TEST_CASE("Roundtrip sRGB<->linear RGB<->LUV", "[fastscaling]") {
    for (int x = 0; x < 256; x++){
	CHECK(x == uchar_clamp_ff(linear_to_srgb(srgb_to_linear((float)x / 255.0f))));
    }
}




TEST_CASE("Roundtrip RGB<->LUV 0,0,0,0 ", "[fastscaling]") {
    float bgra[4] = { 0, 0, 0, 0 };

    linear_to_luv(bgra);
    luv_to_linear(bgra);

    CHECK(bgra[0] == 0.0f);
    CHECK(bgra[1] == 0.0f);
    CHECK(bgra[2] == 0.0f);
}

/*
If we need to research fixed point, convert this test
//Looks like we need an 11 bit integer to safely store a sRGB byte in linear form.

int skipfirst = 0; //Skip the first N values, as if we will handle those individually with branches.
for (int bits = 16; bits > 11; bits--){
    array<Byte, 1>^ from = gcnew array<Byte, 1> (257);
    array<Byte, 1>^ to = gcnew array<Byte, 1> (257);

    for (int x = skipfirst - 1; x < 256; x++){
        float linear = srgb_to_linear (x / 255.0f);

        int divisor = (int)pow (2.0, bits);

        int rounded = lround (linear * divisor);

        from[x + 1] = x;
        to[x + 1] = uchar_clamp_ff (linear_to_srgb ((float)rounded / (float)divisor));
    }
    from[0] = bits;
    to[0] = bits;

    Assert::Equal (from, to);
}*/

SCENARIO("sRGB roundtrip", "[fastscaling]") {
    GIVEN("A 256x256 image, grayscale gradient along the x axis, alpha along the y") {
	int w = 256;
	int h = 256;
	Context context;
	Context_initialize(&context);
	BitmapBgra* bit = create_bitmap_bgra(&context, w, h, true, Bgra32);
	const uint32_t bytes_pp = BitmapPixelFormat_bytes_per_pixel(bit->fmt);

	for (size_t y = 1; y < bit->h; y++){
	    for (size_t x = 0; x < bit->w; x++){
		uint8_t* pix = bit->pixels + (y * bit->stride) + (x * bytes_pp);

		*pix = (uint8_t)x;
		*(pix + 1) = (uint8_t)x;
		*(pix + 2) = (uint8_t)x;
		*(pix + 3) = (uint8_t)y;
	    }
	}

	BitmapBgra* final = create_bitmap_bgra(&context, w, h, true, Bgra32);
	// BitmapFloat* buf = create_bitmap_float(w, h, 4, true);

	WHEN ("we do stuff") {

	    RenderDetails* details = create_render_details();
	    Renderer* r = create_renderer(bit, final, details);

	    perform_render(&context, r);

	    //convert_srgb_to_linear(bit, 0, buf, 0, h);
	    //demultiply_alpha(buf, 0, h);
	    //copy_linear_over_srgb(buf, 0, final, 0, h, 0, buf->w, false);

	    THEN(" and so forth ") {

		bool exact_match = true;
		for (size_t y = 0; y < bit->h; y++){
		    for (size_t x = 0; x < bit->w; x++){
			uint8_t* from = bit->pixels + (y * bit->stride) + (x * bytes_pp);
			uint8_t* to = final->pixels + (y * final->stride) + (x * bytes_pp);

			if (*from != *to) exact_match = false;
			from++; to++;
			if (*from != *to) exact_match = false;
			from++; to++;
			if (*from != *to) exact_match = false;
			from++; to++;
			if (*from != *to) exact_match = false;
			from++; to++;
		    }
		}
		REQUIRE(exact_match);
	    }
	    destroy_renderer(r);
	}
	destroy_bitmap_bgra(final);
	destroy_bitmap_bgra(bit);
	free_lookup_tables();
    }
}

