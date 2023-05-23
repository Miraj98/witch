use std::{vec, io::Write, path::Path, collections::VecDeque};

fn main() {
    let mut args: VecDeque<String> = std::env::args().collect();
    if args.len() < 2 {
        eprintln!("Usage: `libmanger [command] ...args`");
        return;
    }

    let _ = args.pop_front(); // This is the path to this binary
    let command = args.pop_front().unwrap();


    match command.as_str() {
    "compile" | "c" => {
        let mut files_to_compile: Vec<String> = vec![];
        let mut gcc_include_path_args: Vec<String> = vec![];
        let mut header_files: Vec<String> = vec![];
        let mut lib_dir: Option<String> = None;

        /* Read all the args to the `compile` command. */
        while args.len() > 0 {
            let flag = args.pop_front().unwrap();
            match flag.as_str() {
                "--homebrew" | "-hb" => {
                    let mut val = args.pop_front();
                    if val.is_none() {
                        eprintln!("Usage: libmanager {command} {flag} <relative package path> ...");
                        return;
                    }
                    let lib_path = Path::new("/opt/homebrew/Cellar").join(val.take().unwrap());
                    let lib_path_str = lib_path.to_str().unwrap().to_string();
                    gcc_include_path_args.push(String::from("-I"));
                    gcc_include_path_args.push(String::from(lib_path_str));
                },
                _lib_dir => {
                    if lib_dir.is_none() {
                        lib_dir = Some(_lib_dir.to_string());
                    } else {
                        eprintln!("[ERROR] Either {} incorrect arg or {}", _lib_dir, lib_dir.as_ref().unwrap());
                        return;
                    }
                }
            }
        }

        if lib_dir.is_none() {
            eprintln!("[ERROR] Provide a directory to compile to a library.");
            return;
        }

        let lib_dir_name = lib_dir.take().unwrap();

        let lib_dir = Path::new(lib_dir_name.as_str());
        let res = std::fs::read_dir(lib_dir_name.as_str()).expect("Unable to read the dir").collect::<Vec<_>>();
        for i in res.iter() {
            let os_filename = i.as_ref().unwrap().file_name();
            let filename = os_filename.to_str().unwrap();
            if filename.ends_with(".c") {
                files_to_compile.push(lib_dir.join(filename).to_str().unwrap().to_string());
            }
            if filename.ends_with(".h") {
                header_files.push(lib_dir.join(filename).to_str().unwrap().to_string());
            }
        }


        /* Create object files for the library that can be turned into a single archive. */
        let mut gcc_args = vec![String::from("-c")];
        files_to_compile.iter().for_each(|file| {
            gcc_args.push(file.clone());
        });
        gcc_args.append(&mut gcc_include_path_args);
        println!("[INFO] Command: gcc {}", gcc_args.join(" "));
        let out = std::process::Command::new("gcc").args(gcc_args).output();
        if out.is_err() {
            let err = out.err().unwrap();
            eprintln!("...error running previous command {err}");
            return;
        }
        if !out.as_ref().unwrap().status.success() {
            std::io::stderr().write_all(out.as_ref().unwrap().stderr.as_slice()).expect("Writing to stderr should work");
            return;
        }
        
        /* Create the final library archive. */
        let lib_file = filename_with_extension(lib_dir_name.as_str(), ".a");
        let object_files: Vec<String> = files_to_compile.iter().map(|file| {
            let split: Vec<&str> = file.split("/").collect();
            filename_with_extension(split.last().unwrap(), ".o")
        }).collect();
        let mut ar_args = vec![String::from("cr")];
        ar_args.push(lib_file.clone());
        ar_args.append(&mut object_files.clone());
        println!("[INFO] Command: ar {}", ar_args.join(" "));
        let out = std::process::Command::new("ar").args(ar_args).output();
        if out.is_err() {
            let err = out.err().unwrap();
            eprintln!("...error running previous command {err}");
            return;
        }
        if !out.as_ref().unwrap().status.success() {
            std::io::stderr().write_all(out.as_ref().unwrap().stderr.as_slice()).expect("Writing to stderr should work");
            return;
        }

        /* Delete the temporary object files. Just for cleanup. */
        let res = std::fs::read_dir(".").expect("Unable to read the dir").collect::<Vec<_>>();
        for i in res.iter() {
            let os_filename = i.as_ref().unwrap().file_name();
            let filename = os_filename.to_str().unwrap();
            if filename.ends_with(".o") {
                let res = std::fs::remove_file(Path::new(filename));
                if res.is_err() {
                    eprintln!("{}", res.err().unwrap());
                }
            }
        }

        /* Move the library to the `lib` folder or the repository. */
        let out = std::fs::rename(lib_file.as_str(), std::path::Path::new("lib").join(lib_file.as_str()));
        if out.is_err() {
            let err = out.err().unwrap();
            eprintln!("[ERROR] `std::fs::rename` failed {err}");
            return;
        }

        /* Copy the header files from lib_dir to the canonical `include/<lib_dir>/` directory */
        let include_dir = Path::new("include").join(lib_dir_name.as_str());
        if !include_dir.exists() {
            let res = std::fs::create_dir_all(include_dir.as_path());
            if res.is_err() {
                eprintln!("{}", res.err().unwrap());
                return;
            }
        }

        header_files.iter().for_each(|file| {
            let dest = Path::new("include").join(file).to_str().unwrap().to_string();
            let res = std::process::Command::new("cp").args([file.clone(), dest]).output();
            if res.is_err() {
                eprintln!("{}", res.err().unwrap());
                return;
            }
        });
    },
    _ => eprintln!("`{command}` is not a valid a command.")
    }
}

fn file_basename(filename: &str) -> String {
    let split = filename.split(".").collect::<Vec<_>>();
    return split[0].to_string();
}

fn filename_with_extension(filename: &str, extension: &str) -> String {
    let mut basename = file_basename(filename);
    if extension.contains(".") {
        basename.push_str(extension);
    } else {
        basename.push('.');
        basename.push_str(extension);
    }
    return basename;
}
