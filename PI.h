#pragma once
#pragma warning(disable:4996)


#define _USE_MATH_DEFINES
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string>
#include <complex>
#include <algorithm>
#include <memory>
#include <iostream>
#include <list>
#include <thread>
#include <omp.h>
using std::cout;
using std::endl;
using std::complex;

#include "BigFloat.h"
#include "FFT.h"
#include "NTT.h"
#include "Console.h"

#include <chrono>

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
//  Helpers
double wall_clock() {
	auto ratio_object = std::chrono::high_resolution_clock::period();
	double ratio = (double)ratio_object.num / ratio_object.den;
	return std::chrono::high_resolution_clock::now().time_since_epoch().count() * ratio;
}
void dump_to_file(const char* path, const std::string& str) {
	//  Dump a string to a file.

	FILE* file = fopen(path, "wb");
	if (file == NULL)
		throw "Cannot Create File";

	fwrite(str.c_str(), 1, str.size(), file);
	fclose(file);
}

std::string time_str(double s) {
    size_t seconds = (size_t)floor(s);
    size_t minutes = seconds / 60;
    seconds -= minutes * 60;
    return std::to_string(minutes) + " mins, " + std::to_string(seconds) + " seconds";
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
//  Pi

std::atomic_uint64_t iterations;
size_t steps;

void Pi_BSR(BigFloat& P, BigFloat& Q, BigFloat& R, uint32_t a, uint32_t b, size_t p, int tds=1) {
    //  Binary Splitting recursion for the Chudnovsky Formula.

    if (b - a == 1) {
        //  P = (13591409 + 545140134 b)(2b-1)(6b-5)(6b-1) (-1)^b
        P = BigFloat(b).mul(545140134);
        P = P.add(BigFloat(13591409));
        P = P.mul(2 * b - 1);
        P = P.mul(6 * b - 5);
        P = P.mul(6 * b - 1);
        if (b % 2 == 1)
            P.negate();

        //  Q = 10939058860032000 * b^3
        Q = BigFloat(b);
        Q = Q.mul(Q).mul(Q).mul(26726400).mul(409297880);

        //  R = (2b-1)(6b-5)(6b-1)
        R = BigFloat(2 * b - 1);
        R = R.mul(6 * b - 5);
        R = R.mul(6 * b - 1);

        return;
    }

    uint32_t m = (a + b) / 2;

    BigFloat P0, Q0, R0, P1, Q1, R1;
    if (tds == 1) {
        Pi_BSR(P0, Q0, R0, a, m, p);
        Pi_BSR(P1, Q1, R1, m, b, p);
    }
    else {
        int tds0 = tds / 2;
        int tds1 = tds - tds0;
        std::thread left([&P0, &Q0, &R0, a, m, p, tds0]()->void {Pi_BSR(P0, Q0, R0, a, m, p, tds0); });
        std::thread right([&P1, &Q1, &R1, m, b, p, tds1]()->void {Pi_BSR(P1, Q1, R1, m, b, p, tds1); });
        left.join();
        right.join();
    }

    P = P0.mul(Q1, p).add(P1.mul(R0, p), p);
    Q = Q0.mul(Q1, p);
    R = R0.mul(R1, p);

    iterations++;
    const size_t it = iterations;
    std::string s = "\r\x1B[" + std::to_string(WHITE) + "m";
    s += "Summing: ";
    s += "\x1B[" + std::to_string(BRIGHT_CYAN) + "m";
    s += "%.2f%%\t";
    s += "\x1B[" + std::to_string(YELLOW) + "m";
    s += "%llu/%llu";
    s += "\033[0m";
    printf(s.c_str(), ((double)it) / steps * 100, it, steps);
    //printf_color(WHITE, "\rSumming ");
    //printf_color(BRIGHT_CYAN, "%.2f%%, ", ((double)it) / steps * 100);
    //printf_color(YELLOW, "%llu/%llu", it, steps);
}

void Pi(size_t digits, size_t threads) {
    //  The leading 3 doesn't count.

    size_t p = (digits + 9) / 9;
    size_t terms = (size_t)(p * 0.6346230241342037371474889163921741077188431452678) + 1;
    fft_ensure_table(29);
    //ntt_ensure_table(29);
    steps = terms-1;

    //  Limit Exceeded
    if ((uint32_t)terms != terms)
        throw "Limit Exceeded";

    printf_color(WHITE, "Constant:\t");
    printf_color(GREEN, "Pi\n");

    printf_color(WHITE, "Algorithm:\t");
    printf_color(YELLOW, "Chudnovsky (1988)\n\n");

    setlocale(LC_NUMERIC, "");
    printf_color(WHITE, "Decimal Digits:\t");
    printf_color(BRIGHT_GREEN, "%s\n\n", print_num_commas(digits).c_str());

    printf_color(WHITE, "Memory Mode:\t");
    printf_color(RED, "RAM\n");

    printf_color(WHITE, "# Threads:\t");
    printf_color(BRIGHT_YELLOW, "%llu\n\n", threads);

    printf_color(WHITE, "Begin Computation:\n\n");

    printf_color(WHITE, "Summing Series...  %s\n", print_num_commas(terms).c_str());

    double time0 = wall_clock();
    BigFloat P, Q, R;
    Pi_BSR(P, Q, R, 0, (uint32_t)terms, p, threads);
    P = Q.mul(13591409).add(P, p);
    Q = Q.mul(4270934400);
    double time1 = wall_clock();

    printf_color(WHITE, "\n");
    printf_color(CYAN, "%s\n", time_str(time1 - time0).c_str());


    printf_color(WHITE, "Division...\n");
    P = Q.div(P, p);
    double time2 = wall_clock();
    printf_color(CYAN, "%s\n", time_str(time2 - time1).c_str());

    printf_color(WHITE, "InvSqrt...\n");
    Q = invsqrt(10005, p);
    double time3 = wall_clock();
    printf_color(CYAN, "%s\n", time_str(time3 - time2).c_str());

    printf_color(WHITE, "Final Multiply...\n");
    P = P.mul(Q, p);
    double time4 = wall_clock();
    printf_color(CYAN, "%s\n", time_str(time4 - time3).c_str());

    printf_color(WHITE, "Total Time:\t");
    printf_color(CYAN, "%s\n\n", time_str(time4 - time0).c_str());

    dump_to_file("pi.txt", P.to_string(digits+1));
}