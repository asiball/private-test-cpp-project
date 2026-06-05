//! device-ctl — MCP3008 ADC インタラクティブ CLI
//!
//! C++ `cli/src/main.cpp` に相当。
//!
//! C++ との対応:
//!   std::thread + condition_variable の背景モニタ → std::thread + Condvar
//!   atomic<bool> stop フラグ                      → Arc<AtomicBool>
//!   std::mutex + std::unique_lock                  → Arc<Mutex<()>> + Condvar

use std::io::{self, BufRead, Write};
use std::sync::{Arc, Condvar, Mutex};
use std::sync::atomic::{AtomicBool, Ordering};
use std::time::Duration;

use clap::Parser;
use libsensor::Mcp3008;

#[derive(Parser, Debug)]
#[command(name = "device-ctl", about = "MCP3008 ADC 制御ツール (Rust 版)")]
struct Args {
    /// SPI デバイスパス
    #[arg(short, long, default_value = "/dev/spidev0.0")]
    device: String,

    /// 基準電圧 [V]
    #[arg(long, default_value_t = 3.3)]
    vref: f64,
}

fn main() {
    embedded_common::init_logger();
    let args = Args::parse();

    let mut sensor = Mcp3008::new(&args.device, args.vref);
    if let Err(e) = sensor.open() {
        eprintln!("エラー: センサーを開けませんでした: {e}");
        std::process::exit(1);
    }

    println!("device-ctl (Rust) — デバイス: {} vref: {}V", args.device, args.vref);

    // ---- 背景モニタスレッド (C++ の monitor thread に相当) ----
    // Arc<Mutex> + Arc<Condvar> で60秒ごとに CH0 を読む
    let stop_flag = Arc::new(AtomicBool::new(false));
    let pair = Arc::new((Mutex::new(false), Condvar::new()));

    {
        let stop = Arc::clone(&stop_flag);
        let pair2 = Arc::clone(&pair);
        let device = args.device.clone();
        let vref = args.vref;

        std::thread::spawn(move || {
            let mut bg_sensor = Mcp3008::new(&device, vref);
            if bg_sensor.open().is_err() {
                return;
            }

            let (lock, cvar) = &*pair2;
            loop {
                // 60 秒待機 (停止通知で即時抜け出す)
                let guard = lock.lock().unwrap();
                let (_guard, _timed_out) = cvar
                    .wait_timeout(guard, Duration::from_secs(60))
                    .unwrap();

                if stop.load(Ordering::Relaxed) {
                    break;
                }
                match bg_sensor.read_voltage(0) {
                    Ok(v) => println!("[モニタ] CH0: {:.4} V", v),
                    Err(e) => eprintln!("[モニタ] 読み出しエラー: {e}"),
                }
            }
        });
    }

    // ---- インタラクティブメニュー ----
    let stdin = io::stdin();
    loop {
        print!("\n[1] チャンネル指定読み出し  [2] 全チャンネルスキャン  [q] 終了 > ");
        io::stdout().flush().ok();

        let mut line = String::new();
        if stdin.lock().read_line(&mut line).is_err() {
            break;
        }
        match line.trim() {
            "1" => {
                print!("チャンネル番号 (0–7): ");
                io::stdout().flush().ok();
                let mut ch_str = String::new();
                if stdin.lock().read_line(&mut ch_str).is_err() {
                    continue;
                }
                let ch: u8 = match ch_str.trim().parse() {
                    Ok(v) => v,
                    Err(_) => { eprintln!("無効な入力"); continue; }
                };
                match sensor.read_voltage(ch) {
                    Ok(v) => println!("CH{}: {:.4} V", ch, v),
                    Err(e) => eprintln!("エラー: {e}"),
                }
            }
            "2" => {
                for ch in 0..8u8 {
                    match sensor.read_voltage(ch) {
                        Ok(v) => println!("CH{}: {:.4} V", ch, v),
                        Err(e) => eprintln!("CH{} エラー: {e}", ch),
                    }
                }
            }
            "q" | "Q" => break,
            _ => eprintln!("不明なコマンドです"),
        }
    }

    // 背景スレッドを停止
    stop_flag.store(true, Ordering::Relaxed);
    pair.1.notify_all();
    println!("終了しました");
}
