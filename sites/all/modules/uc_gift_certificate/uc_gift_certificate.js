

var gift_cert_initialized = false;
var using_gift_certs = false;

// Adds a click function to the total so we can check its updated values.
$(document).ready(
    function() {
        $('#edit-panes-payment-current-total').click(function() { uc_gift_certificate_check_total(this.value); });
    }
);

/**
 * Checks the current total and updates the available/selected payment methods
 * accordingly.
 */
function uc_gift_certificate_check_total(total) {
    total = parseFloat(total);

    // Disable the free order option and select the first available method.
    if (total >= .005 && (using_gift_certs != false || gift_cert_initialized == false)) {

        using_gift_certs = false;
    }
    else if (total < .005 && using_free_order != true) {
        // Hide the fallback payment method radio.
        //$("#payment-pane .form-radios input:radio").attr('disabled', 'disabled').parent().hide(0);

        // Show and select the Gift Certificate payment method.
        $("input:radio[value=zero_total]").removeAttr('disabled').attr('checked', 'checked').parent().show(0);

        // Refresh the payment details section.
        get_payment_details(Drupal.settings.ucURL.checkoutPaymentDetails + 'zero_total');

        using_gift_certs = true;
    }

    gift_cert_initialized = true;
}

