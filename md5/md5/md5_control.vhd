library IEEE;
use IEEE.std_logic_1164.all;
use IEEE.numeric_std.all;

entity md5_control is
    port (
        avs_s0_address   : in  std_logic_vector(3 downto 0)  := (others => '0'); -- avalon slave interface
        avs_s0_write     : in  std_logic                     := '0';
        avs_s0_writedata : in  std_logic_vector(31 downto 0) := (others => '0');
        avs_s0_read      : in  std_logic                     := '0';
        avs_s0_readdata  : out std_logic_vector(31 downto 0);
        clk              : in  std_logic                     := '0';             -- clock
        reset            : in  std_logic                     := '0';             -- reset
        md5_start        : out std_logic_vector(31 downto 0);                   -- control signals to MD5 core
        md5_reset        : out std_logic_vector(31 downto 0);
        md5_wr           : out std_logic;                                      -- write enable
        md5_done         : in  std_logic_vector(31 downto 0) := (others => '0') -- status from MD5 core
    );
end entity md5_control;

architecture rtl of md5_control is
    SIGNAL start_reg, reset_reg : STD_LOGIC_VECTOR(31 DOWNTO 0);
    SIGNAL wr_reg : STD_LOGIC;
begin
    PROCESS(clk, reset)
    BEGIN
        IF(reset = '1') THEN
            start_reg <= (OTHERS => '0');
            reset_reg <= (OTHERS => '0');
            wr_reg <= '0';
            avs_s0_readdata <= (OTHERS => '0');
        ELSIF(rising_edge(clk)) THEN
            wr_reg <= '0'; -- default value
            
            IF(avs_s0_write = '1') THEN
                CASE avs_s0_address IS
                    WHEN "0000" => -- Start register
                        start_reg <= avs_s0_writedata;
                    WHEN "0001" => -- Reset register 
                        reset_reg <= avs_s0_writedata;
                    WHEN "0010" => -- Write control
                        wr_reg <= avs_s0_writedata(0);
                    WHEN OTHERS =>
                        -- Do nothing
                END CASE;            
            ELSIF(avs_s0_read = '1') THEN
                CASE avs_s0_address IS
                    WHEN "0000" => -- Start status
                        avs_s0_readdata <= start_reg;
                    WHEN "0001" => -- Reset status
                        avs_s0_readdata <= reset_reg;
                    WHEN "0010" => -- Done status
                        avs_s0_readdata <= md5_done;
                    WHEN OTHERS => 
                        avs_s0_readdata <= (OTHERS => '0');
                END CASE;
            END IF;
        END IF;
    END PROCESS;

    md5_start <= start_reg;
    md5_reset <= reset_reg;
    md5_wr <= wr_reg;
    
end architecture rtl;
