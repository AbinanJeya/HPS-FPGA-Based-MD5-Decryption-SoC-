library IEEE;
use IEEE.std_logic_1164.all;
use IEEE.numeric_std.all;

entity md5_data is
    port (
        avs_s0_address   : in  std_logic_vector(3 downto 0)  := (others => '0'); -- avalon slave interface
        avs_s0_read      : in  std_logic                     := '0';
        avs_s0_write     : in  std_logic                     := '0';
        avs_s0_readdata  : out std_logic_vector(31 downto 0);
        avs_s0_writedata : in  std_logic_vector(31 downto 0) := (others => '0');
        clk              : in  std_logic                     := '0';             -- clock
        reset            : in  std_logic                     := '0';             -- reset
        md5_writedata    : out std_logic_vector(31 downto 0);                   -- data to MD5 core
        md5_writeaddr    : out std_logic_vector(31 downto 0);                   -- write address
        md5_readaddr     : out std_logic_vector(31 downto 0);                   -- read address
        md5_readdata     : in  std_logic_vector(31 downto 0) := (others => '0') -- data from MD5 core
    );
end entity md5_data;

architecture rtl of md5_data is
    SIGNAL writedata_reg : STD_LOGIC_VECTOR(31 DOWNTO 0);
    SIGNAL writeaddr_reg : STD_LOGIC_VECTOR(31 DOWNTO 0);
    SIGNAL readaddr_reg : STD_LOGIC_VECTOR(31 DOWNTO 0);
begin
    PROCESS(clk, reset)
    BEGIN
        IF(reset = '1') THEN
            writedata_reg <= (OTHERS => '0');
            writeaddr_reg <= (OTHERS => '0');
            readaddr_reg <= (OTHERS => '0');
            avs_s0_readdata <= (OTHERS => '0');
        ELSIF(rising_edge(clk)) THEN
            IF(avs_s0_read = '1') THEN
                CASE avs_s0_address IS
                    WHEN "0000" => -- Read data from MD5 core
                        avs_s0_readdata <= md5_readdata;
                    WHEN "0001" => -- Current write data
                        avs_s0_readdata <= writedata_reg;
                    WHEN "0010" => -- Current write address
                        avs_s0_readdata <= writeaddr_reg;
                    WHEN "0011" => -- Current read address
                        avs_s0_readdata <= readaddr_reg;
                    WHEN OTHERS =>
                        avs_s0_readdata <= (OTHERS => '0');
                END CASE;
            ELSIF(avs_s0_write = '1') THEN
                CASE avs_s0_address IS
                    WHEN "0000" => -- Write data
                        writedata_reg <= avs_s0_writedata;
                    WHEN "0001" => -- Write address
                        writeaddr_reg <= avs_s0_writedata;
                    WHEN "0010" => -- Read address
                        readaddr_reg <= avs_s0_writedata;
                    WHEN OTHERS =>
                        -- Do nothing
                END CASE;
            END IF;
        END IF;
    END PROCESS;
    
    md5_writedata <= writedata_reg;
    md5_writeaddr <= writeaddr_reg;
    md5_readaddr <= readaddr_reg;
    
end architecture rtl;
