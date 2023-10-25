#!/usr/bin/env python3

import os
import sys

def split_pem_certificates(input_file, outdir):

    if not outdir:
        outdir = os.path.dirname(input_file)

    if not os.path.exists(outdir):
        os.makedirs(outdir)

    if os.path.exists(input_file):
        print(f"**** Splitting Certificate File {input_file} into {outdir} ...")

        with open(input_file, "r") as file:
            certificates = file.read().split("-----END CERTIFICATE-----\n")
            
            for index, certificate in enumerate(certificates):
                if len(certificate) > 0:
                    certificate += "-----END CERTIFICATE-----\n"
                    
                    output_file = f"orgcert_{index + 1}.pem"
                    output_file = os.path.join(outdir, output_file)
                    with open(output_file, "w") as output:
                        output.write(certificate)

                    print(f"Certificate {output_file} saved")
    else:
        print(f"Certificate File {input_file} not found - no splitting carried out")

argcount = len(sys.argv)
if argcount > 1:
    input_file = sys.argv[1]
    outdir = sys.argv[2] if argcount > 2 else None

    split_pem_certificates(input_file, outdir)
else:
    print("No file name provided to split certificates")